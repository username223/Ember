/*
 * Copyright (c) 2015, 2016 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <shared/database/Exception.h>
#include <shared/database/objects/User.h>
#include <boost/optional.hpp>
#include <unordered_map>
#include <string>
#include <cstdint>

namespace ember { namespace dal {

class UserDAO {
public:
	virtual boost::optional<User> user(const std::string& username) const = 0;
	virtual void record_last_login(std::uint32_t account_id, const std::string& ip) const = 0;
	virtual std::unordered_map<std::uint32_t, std::uint32_t> character_counts(std::uint32_t account_id) const = 0;
	virtual ~UserDAO() = default;
};

}} //dal, ember