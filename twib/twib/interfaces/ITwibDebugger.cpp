//
// Twili - Homebrew debug monitor for the Nintendo Switch
// Copyright (C) 2018 misson20000 <xenotoad@xenotoad.net>
//
// This file is part of Twili.
//
// Twili is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Twili is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Twili.  If not, see <http://www.gnu.org/licenses/>.
//

#include "ITwibDebugger.hpp"

#include<string.h>
#include "Protocol.hpp"

namespace twili {
namespace twib {

ITwibDebugger::ITwibDebugger(std::shared_ptr<RemoteObject> obj) : obj(obj) {
	
}

nx::DebugEvent ITwibDebugger::GetDebugEvent() {
	nx::DebugEvent event;
	memset(&event, 0, sizeof(event));
	std::vector<uint8_t> payload = obj->SendSyncRequest(protocol::ITwibDebugger::Command::GET_DEBUG_EVENT).payload;
	memcpy(&event, payload.data(), payload.size());
	return event;
}

std::vector<uint64_t> ITwibDebugger::GetThreadContext(uint64_t thread_id) {
	uint8_t *tid_bytes = (uint8_t*) &thread_id;
	std::vector<uint8_t> payload = obj->SendSyncRequest(protocol::ITwibDebugger::Command::GET_THREAD_CONTEXT, std::vector<uint8_t>(tid_bytes, tid_bytes + sizeof(thread_id))).payload;
	return std::vector<uint64_t>((uint64_t*) payload.data(), (uint64_t*) (payload.data() + payload.size()));
}

} // namespace twib
} // namespace twili