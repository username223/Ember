/*
 * Copyright (c) 2016 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ClientConnection.h"
#include <game_protocol/Packets.h>
#include <spark/SafeBinaryStream.h>
#include <botan/bigint.h>
#include <botan/sha160.h>

#undef ERROR // temp

namespace em = ember::messaging;

namespace ember {

void ClientConnection::send_auth_challenge() {
	auto packet = std::make_shared<protocol::SMSG_AUTH_CHALLENGE>();
	packet->seed = auth_seed_ = 600; // todo, obviously

	send(protocol::ServerOpcodes::SMSG_AUTH_CHALLENGE, packet);
	state_ = ClientStates::AUTHENTICATING;
}

void ClientConnection::fetch_session_key(const protocol::CMSG_AUTH_SESSION& packet) {
	LOG_TRACE_FILTER(logger_, LF_NETWORK) << __func__ << LOG_ASYNC;
	LOG_DEBUG(logger_) << "Received session proof from " << packet.username << LOG_ASYNC;

	auto self = shared_from_this();

	acct_serv->locate_session(packet.username,
		[this, self, packet](em::account::Status remote_res, Botan::BigInt key) {
			service_.post([this, self, packet, remote_res, key]() {
				LOG_DEBUG_FILTER(logger_, LF_NETWORK)
					<< "Account server returned " << em::account::EnumNameStatus(remote_res)
					<< " for " << packet.username << LOG_ASYNC;

				if(remote_res != em::account::Status::OK) {
					protocol::ResultCode result;

					switch(remote_res) {
						case em::account::Status::ALREADY_LOGGED_IN:
							result = protocol::ResultCode::AUTH_ALREADY_ONLINE;
							break;
						case em::account::Status::SESSION_NOT_FOUND:
							result = protocol::ResultCode::AUTH_UNKNOWN_ACCOUNT;
							break;
						default:
							LOG_ERROR_FILTER(logger_, LF_NETWORK) << "Received "
								<< em::account::EnumNameStatus(remote_res)
								<< " from account server" << LOG_ASYNC;
							result = protocol::ResultCode::AUTH_SYSTEM_ERROR;
					}

					send_auth_fail(result);
				} else {
					prove_session(key, packet);
				}
			});
	});
}

void ClientConnection::prove_session(Botan::BigInt key, const protocol::CMSG_AUTH_SESSION& packet) {
	Botan::SecureVector<Botan::byte> k_bytes = Botan::BigInt::encode(key);
	std::uint32_t unknown = 0;

	Botan::SHA_160 hasher;
	hasher.update(packet.username);
	hasher.update((Botan::byte*)&unknown, 4);
	hasher.update((Botan::byte*)&packet.seed, 4);
	hasher.update((Botan::byte*)&auth_seed_, 4);
	hasher.update(k_bytes);
	Botan::SecureVector<Botan::byte> calc_hash = hasher.final();

	auto response = std::make_shared<protocol::SMSG_AUTH_RESPONSE>();

	if(calc_hash != packet.digest) {
		send_auth_fail(protocol::ResultCode::AUTH_BAD_SERVER_PROOF);
		return;
	}

	crypto_.set_key((char*)k_bytes.begin());
	authenticated_ = true;

	/* 
	 * Note: MaNGOS claims you need a full auth packet for the initial AUTH_WAIT_QUEUE
	 * but that doesn't seem to be true - if this bugs out, check that out
	 */
	if(2 > 1) { // add server population check
		state_ = ClientStates::IN_QUEUE;
		queue_service_temp->enqueue(shared_from_this());
		return;
	}

	/*auto resp = std::make_shared<Packet>();
	PacketStream<Packet> stream(resp.get());
	stream << boost::endian::native_to_big(std::uint16_t(12)) << grunt::server::Opcode::SMSG_AUTH << std::uint8_t(12)
		<< std::uint32_t(0) << std::uint8_t(0) << std::uint32_t(0);
	crypt_.encrypt(resp->data());
	write(resp);
	state_ = State::AUTHED;*/
}

void ClientConnection::send_auth_fail(protocol::ResultCode result) {
	LOG_TRACE_FILTER(logger_, LF_NETWORK) << __func__ << LOG_ASYNC;

	// not convinced that this packet is correct
	auto response = std::make_shared<protocol::SMSG_AUTH_RESPONSE>();
	response->result = result;
	send(protocol::ServerOpcodes::SMSG_AUTH_RESPONSE, response);
	close_session();
}

void ClientConnection::handle_authentication(spark::Buffer& buffer) {
	LOG_TRACE_FILTER(logger_, LF_NETWORK) << __func__ << LOG_ASYNC;

	if(packet_header_.opcode != protocol::ClientOpcodes::CMSG_AUTH_SESSION) {
		LOG_DEBUG_FILTER(logger_, LF_NETWORK)
			<< "Expected CMSG_AUTH_CHALLENGE, dropping "
			<< remote_address() << remote_port() << LOG_ASYNC;
		close_session();
		return;
	}

	spark::SafeBinaryStream stream(buffer);
	protocol::CMSG_AUTH_SESSION packet;

	if(packet.read_from_stream(stream) != protocol::Packet::State::DONE) {
		LOG_DEBUG_FILTER(logger_, LF_NETWORK)
			<< "Authentication packet parse failed, disconnecting" << LOG_ASYNC;
		close_session();
		return;
	}

	fetch_session_key(packet);
}

void ClientConnection::dispatch_packet(spark::Buffer& buffer) {
	switch(state_) {
		case ClientStates::AUTHENTICATING:
			handle_authentication(buffer);
			break;
		case ClientStates::IN_QUEUE:
			break;
		case ClientStates::CHARACTER_LIST:
			break;
		case ClientStates::IN_WORLD:
			break;
		default:
			// assert
			break;
	}
}

void ClientConnection::parse_header(spark::Buffer& buffer) {
	if(buffer.size() < sizeof(protocol::ClientHeader)) {
		return;
	}

	buffer.read(&packet_header_.size, sizeof(protocol::ClientHeader::size));
	buffer.read(&packet_header_.opcode, sizeof(protocol::ClientHeader::opcode));

	if(authenticated_) {
		crypto_.decrypt((char*)&packet_header_.size, sizeof(protocol::ClientHeader::size));
		crypto_.decrypt((char*)&packet_header_.opcode, sizeof(protocol::ClientHeader::opcode));
	}

	read_state_ = ReadState::BODY;
}

void ClientConnection::completion_check(spark::Buffer& buffer) {
	if(buffer.size() < packet_header_.size - sizeof(protocol::ClientHeader::opcode)) {
		return;
	}

	read_state_ = ReadState::DONE;
}

bool ClientConnection::handle_packet(spark::Buffer& buffer) {
	if(read_state_ == ReadState::HEADER) {
		parse_header(buffer);
	}

	if(read_state_ == ReadState::BODY) {
		completion_check(buffer);
	}

	if(read_state_ == ReadState::DONE) {
		dispatch_packet(buffer);
		read_state_ = ReadState::HEADER;
	}

	return true; // temp
}

void ClientConnection::start() {
	send_auth_challenge();
	read();
}

boost::asio::ip::tcp::socket& ClientConnection::socket() {
	return socket_;
}

std::string ClientConnection::remote_address() {
	return socket_.remote_endpoint().address().to_string();
}

std::uint16_t ClientConnection::remote_port() {
	return socket_.remote_endpoint().port();
}

void ClientConnection::close_session() {
	switch(state_) {
		case ClientStates::CHARACTER_LIST:
		case ClientStates::IN_WORLD:
			queue_service_temp->decrement();
			break;
		case ClientStates::IN_QUEUE:
			queue_service_temp->dequeue(shared_from_this());
			break;
	}

	sessions_.stop(shared_from_this());
}

void ClientConnection::send(protocol::ServerOpcodes opcode, std::shared_ptr<protocol::Packet> packet) {
	auto self(shared_from_this());

	service_.dispatch([this, self, opcode, packet]() {
		std::uint16_t size = boost::endian::native_to_big(std::uint16_t(packet->size() + sizeof(opcode)));

		if(authenticated_) {
			crypto_.encrypt((char*)&size, sizeof(protocol::ServerHeader::size));
			crypto_.encrypt((char*)&opcode, sizeof(protocol::ServerHeader::opcode));
		}

		spark::SafeBinaryStream stream(outbound_buffer_);
		stream << size << opcode;
		packet->write_to_stream(stream);
		write();
	});
}

void ClientConnection::write() {
	auto self(shared_from_this());

	if(!socket_.is_open()) {
		return;
	}

	socket_.async_send(outbound_buffer_, create_alloc_handler(allocator_,
		[this, self](boost::system::error_code ec, std::size_t size) {
			outbound_buffer_.skip(size);

			if(ec && ec != boost::asio::error::operation_aborted) {
				close_session();
			}
		}
	));
}

void ClientConnection::read() {
	auto self(shared_from_this());
	auto tail = inbound_buffer_.tail();

	// if the buffer chain has no more space left, allocate & attach new node
	if(!tail->free()) {
		tail = inbound_buffer_.allocate();
		inbound_buffer_.attach(tail);
	}

	socket_.async_receive(boost::asio::buffer(tail->data(), tail->free()),
		create_alloc_handler(allocator_,
		[this, self](boost::system::error_code ec, std::size_t size) {
			if(stopped_) {
				return;
			}

			if(!ec) {
				inbound_buffer_.advance_write_cursor(size);

				if(handle_packet(inbound_buffer_)) {
					read();
				} else {
					close_session();
				}
			} else if(ec != boost::asio::error::operation_aborted) {
				close_session();
			}
		}
	));
}

void ClientConnection::stop() {
	auto self(shared_from_this());

	socket_.get_io_service().post([this, self] {
		LOG_DEBUG_FILTER(logger_, LF_NETWORK)
			<< "Closing connection to " << remote_address()
			<< ":" << remote_port() << LOG_ASYNC;

		stopped_ = true;
		boost::system::error_code ec; // we don't care about any errors
		socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
		socket_.close(ec);
	});
}

} // ember