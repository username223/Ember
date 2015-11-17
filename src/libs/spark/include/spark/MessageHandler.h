/*
 * Copyright (c) 2015 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <spark/Link.h>
#include <spark/temp/MessageRoot_generated.h>
#include <logger/Logging.h>
#include <vector>
#include <cstdint>

namespace ember { namespace spark {

class NetworkSession;

class MessageHandler {
	enum class State {
		HANDSHAKING, NEGOTIATING, FORWARDING
	} state_ = State::HANDSHAKING;

	Link peer_;
	const Link& self_;
	log::Logger* logger_;
	log::Filter filter_;
	bool initiator_;

	bool negotiate_protocols(NetworkSession& net, const messaging::MessageRoot* message);
	bool establish_link(NetworkSession& net, const messaging::MessageRoot* message);
	void send_banner(NetworkSession& net);
	void send_negotiation(NetworkSession& net);

public:
	MessageHandler(const Link& link, log::Logger* logger, log::Filter filter);

	bool handle_message(NetworkSession& net, const std::vector<std::uint8_t>& buffer);
	void start(NetworkSession& net);
};

}} // spark, ember