/*
 * Copyright (c) 2016 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "WorldClients.h"

namespace ember {

void WorldClients::add(boost::uuids::uuid uuid, std::shared_ptr<ClientConnection> client) {

}

void WorldClients::remove(boost::uuids::uuid uuid) {

}

void WorldClients::remove(std::shared_ptr<ClientConnection> client) {

}

std::shared_ptr<ClientConnection> locate(boost::uuids::uuid uuid) {
	return nullptr;
}

} // ember