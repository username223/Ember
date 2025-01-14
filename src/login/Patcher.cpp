/*
 * Copyright (c) 2015 - 2021 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Patcher.h"
#include "PatchGraph.h"
#include <shared/util/FileMD5.h>
#include <shared/util/FNVHash.h>
#include <boost/endian/conversion.hpp>
#include <algorithm>
#include <fstream>

namespace ember {

Patcher::Patcher(std::vector<GameVersion> versions, std::vector<PatchMeta> patches)
                 : versions_(std::move(versions)), patches_(std::move(patches)),
                   survey_id_(0) {
	FNVHash hasher;

	for(auto& patch : patches_) {
		hasher.update(patch.locale);
		hasher.update(patch.arch);
		hasher.update(patch.os);
		auto hash = hasher.finalise();
		patch_bins[hash].emplace_back(patch);
	}

	for(auto& [size, meta] : patch_bins) {
		graphs_.emplace(size, PatchGraph(meta));
	}
}

const PatchMeta* Patcher::locate_rollup(const std::vector<PatchMeta>& patches,
                                        std::uint16_t from, std::uint16_t to) const {
	const PatchMeta* meta = nullptr;

	for(auto& patch : patches) {
		if(!patch.rollup) {
			continue;
		}

		// rollup build must be <= the client build and <= the server build
		if(patch.build_from <= from && patch.build_to <= to) {
			if(meta == nullptr) {
				meta = &patch;
			} else if(meta->file_meta.size >= patch.file_meta.size) {
				meta = &patch; // go for the smaller file
			}
		}
	}

	return meta;
}

std::optional<PatchMeta> Patcher::find_patch(const GameVersion& client_version, grunt::Locale locale,
                                             grunt::Platform platform, grunt::System os) const {
	FNVHash hasher;
	hasher.update(grunt::to_string(locale));
	hasher.update(grunt::to_string(platform));
	auto hash = hasher.update(grunt::to_string(os));

	auto g_it = graphs_.find(hash);
	auto p_it = patch_bins.find(hash);
	
	if(g_it == graphs_.end() || p_it == patch_bins.end()) {
		return std::nullopt;
	}

	auto build = client_version.build;
	bool path = false;

	// ensure there's a patch path from the client version to a supported version
	for(auto& version : versions_) {
		if(g_it->second.is_path(build, version.build)) {
			path = true;
			break;
		}
	}

	// couldn't find a patch path, find the best rollup patch that'll cover the client
	if(!path) {
		const PatchMeta* meta = nullptr;

		for(auto& version : versions_) {
			meta = locate_rollup(p_it->second, client_version.build, version.build);

			// check to see whether there's a patch path from this rollup
			if(meta && g_it->second.is_path(meta->build_from, version.build)) {
				build = meta->build_from;
				path = true;
				break;
			}
		}

		// still no path? Guess we're out of luck.
		if(!path) {
			return std::nullopt;
		}
	}

	// using the optimal patching path, locate the next patch file
	for(auto& version : versions_) {
		auto paths = g_it->second.path(build, version.build);
		
		if(paths.empty()) {
			continue;
		}
		
		auto build_to = version.build;
		auto build_from = paths.front().from;
		paths.pop_front();

		if(!paths.empty()) {
			build_to = paths.front().from;
		}

		for(auto& patch : p_it->second) {
			if(patch.build_from == build_from && patch.build_to == build_to) {
				return patch;
			}
		}
	}

	return std::nullopt;
}

auto Patcher::check_version(const GameVersion& client_version) const -> PatchLevel {
	if(std::find(versions_.begin(), versions_.end(), client_version) != versions_.end()) {
		return PatchLevel::OK;
	}

	// Figure out whether any of the allowed client versions are newer than the client.
	// If so, there's a chance that it can be patched.
	for(auto& v : versions_) {
		if(v > client_version) {
			return PatchLevel::TOO_OLD;
		}
	}

	return PatchLevel::TOO_NEW;
}

void Patcher::set_survey(const std::string& path, std::uint32_t id) {
	survey_.name = "Survey";
	survey_id_ = id;
	
	std::ifstream file(path + "Survey.mpq", std::ios::binary | std::ios::ate);

	if(!file) {
		throw std::runtime_error("Error opening " + path + "Survey.mpq");
	}

	survey_.size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<std::byte> buffer(static_cast<std::size_t>(survey_.size));
	file.read(reinterpret_cast<char*>(buffer.data()), survey_.size);

	if(!file.good()) {
		throw std::runtime_error("An error occured while reading " + path + "Survey.mpq");
	}

	auto md5 = util::generate_md5(*buffer.data(), buffer.size());
	std::copy(md5.begin(), md5.end(), reinterpret_cast<unsigned char*>(survey_.md5.data()));
	survey_data_ = std::move(buffer);
}

FileMeta Patcher::survey_meta() const {
	return survey_;
}

// todo, only supports x86 Windows for the time being
bool Patcher::survey_platform(grunt::Platform platform, grunt::System os) const {
	if(platform != grunt::Platform::x86 || os != grunt::System::Win) {
		return false;
	}

	return !survey_data(platform, os).empty();
}

// todo, change how this works
const std::vector<std::byte>& Patcher::survey_data(grunt::Platform platform, grunt::System os) const {
	return survey_data_;
}

std::uint32_t Patcher::survey_id() const {
	return survey_id_;
}

std::vector<PatchMeta> Patcher::load_patches(const std::string& path, const dal::PatchDAO& dao,
                                             log::Logger* logger) {
	auto patches = dao.fetch_patches();

	for(auto& patch : patches) {
		bool dirty = false;
		patch.file_meta.path = path;

		// we open each patch to make sure that it at least exists
		std::ifstream file(path + patch.file_meta.name, std::ios::binary | std::ios::ate);

		if(!file) {
			throw std::runtime_error("Error opening patch " + path + patch.file_meta.name);
		}

		if(patch.file_meta.size == 0) {
			patch.file_meta.size = static_cast<std::uint64_t>(file.tellg());

			if(!file.good()) {
				throw std::runtime_error("Unable determine patch size for " + path + patch.file_meta.name);
			}

			dirty = true;
		}

		// check whether the hash is all zeroes and calculate it if so
		const auto calc_md5 = std::all_of(patch.file_meta.md5.begin(), patch.file_meta.md5.end(),
			[](const auto& byte) { return byte == std::byte{ 0 }; }
		);

		if(calc_md5) {
			LOG_INFO(logger) << "Calculating MD5 for " << patch.file_meta.name << LOG_SYNC;
			auto md5 = util::generate_md5(path + patch.file_meta.name);
			std::copy(md5.begin(), md5.end(), reinterpret_cast<unsigned char*>(patch.file_meta.md5.data()));
			dirty = true;
		}

		if(dirty) {
			dao.update(patch);
		}
	}

	return patches;
}

} // ember