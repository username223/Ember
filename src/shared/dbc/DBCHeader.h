/*
 * Copyright (c) 2014, 2016 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <cstdint>

namespace ember { namespace dbc {

#pragma pack(push, 1)

struct DBCHeader {
	std::uint32_t magic;
	std::uint32_t records;
	std::uint32_t fields;
	std::uint32_t record_size;
	std::uint32_t string_block_len;
};

#pragma pack(pop)

}} // dbc, ember