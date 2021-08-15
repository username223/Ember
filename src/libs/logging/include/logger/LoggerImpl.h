/*
 * Copyright (c) 2015 - 2021 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <logger/HelperMacros.h>
#include <logger/Worker.h>
#include <logger/Sink.h>
#include <logger/Severity.h>
#include <logger/Logger.h>
#include <logger/concurrentqueue.h>
#include <shared/threading/Semaphore.h>
#include <algorithm>
#include <iterator>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <utility>
#include <tuple>
#include <condition_variable>
#include <thread>
#include <cstddef>
#include <cstring>

namespace ember::log {

class Logger::impl final {
	friend class Logger;

	static constexpr std::size_t BUFFER_RESERVE = 512;

	Severity severity_ = Severity::DISABLED;
	Filter filter_ = Filter(0);
	std::vector<std::unique_ptr<Sink>> sinks_;
	Worker worker_;

	static thread_local std::pair<RecordDetail, std::vector<char>> buffer_;
	static thread_local Semaphore<std::mutex> sem_;

	void finalise() {
		buffer_.second.push_back('\n');
		worker_.queue_.enqueue(std::move(buffer_));
		worker_.signal();
		buffer_.second.reserve(BUFFER_RESERVE);
	}

	void finalise_sync() {
		buffer_.second.push_back('\n');
		auto r = std::make_tuple<RecordDetail, std::vector<char>, Semaphore<std::mutex>*>
					(std::move(buffer_.first), std::move(buffer_.second), &sem_);
		worker_.queue_sync_.enqueue(std::move(r));
		worker_.signal();
		buffer_.second.reserve(BUFFER_RESERVE);
		sem_.wait();
	}

	std::vector<char>* get_buffer() {
		return &buffer_.second;
	}

public:
	impl() : worker_(sinks_) {
#ifndef DEBUG_NO_THREADS
		worker_.start();
#endif
	}

	~impl() {
#ifndef DEBUG_NO_THREADS
		worker_.stop();
#endif
	}

	template<typename T>
	void copy_to_stream(T& data) {
		std::string conv = std::to_string(data);
		std::copy(conv.begin(), conv.end(), std::back_inserter(buffer_.second));
	}

	impl& operator <<(impl& (*m)(impl&)) {
		buffer_.first.type = 1; // first bit represents the misc. record type
		return (*m)(*this);
	}

	impl& operator <<(Severity severity) {
		buffer_.first.type = 1; // first bit represents the misc. record type
		buffer_.first.severity = severity;
		return *this;
	}

	impl& operator <<(Filter type) {
		buffer_.first.type = type;
		return *this;
	}

	impl& operator <<(float data) {
		copy_to_stream(data);
		return *this;
	}

	impl& operator <<(double data) {
		copy_to_stream(data);
		return *this;
	}

	impl& operator <<(bool data) {
		copy_to_stream(data);
		return *this;
	}

	impl& operator <<(int data) {
		copy_to_stream(data);
		return *this;
	}

	impl& operator <<(unsigned int data) {
		copy_to_stream(data);
		return *this;
	}

	impl& operator <<(long data) {
		copy_to_stream(data);
		return *this;
	}

	impl& operator <<(unsigned long data) {
		copy_to_stream(data);
		return *this;
	}

	impl& operator <<(unsigned long long data) {
		copy_to_stream(data);
		return *this;
	}

	impl& operator <<(long long data) {
		copy_to_stream(data);
		return *this;
	}

	impl& operator <<(const std::string& data) {
		std::copy(data.begin(), data.end(), std::back_inserter(buffer_.second));
		return *this;
	}

	impl& operator <<(const std::string_view data) {
		std::copy(data.begin(), data.end(), std::back_inserter(buffer_.second));
		return *this;
	}

	impl& operator <<(const char* data) {
		std::copy(data, data + std::strlen(data), std::back_inserter(buffer_.second));
		return *this;
	}

	Severity severity() {
		return severity_;
	}

	Filter filter() {
		return filter_;
	}

	void add_sink(std::unique_ptr<Sink> sink) {
		if(sink->severity() < severity_) {
			severity_ = sink->severity();
		}

		filter_ |= sink->filter();
		sinks_.emplace_back(std::move(sink));
	}

	impl(const impl&) = delete;
	impl& operator=(const impl&) = delete;
};

thread_local std::pair<RecordDetail, std::vector<char>> Logger::impl::buffer_;
thread_local Semaphore<std::mutex> Logger::impl::sem_;

} //log, ember