//
// Twili - Homebrew debug monitor for the Nintendo Switch
// Copyright (C) 2019 misson20000 <xenotoad@xenotoad.net>
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

#include "ECSProcess.hpp"

#include "../twili.hpp"
#include "fs/ActualFile.hpp"
#include "fs/NRONSOTransmutationFile.hpp"

#include "err.hpp"
#include "title_id.hpp"

using namespace trn;

namespace twili {
namespace process {

ECSProcess::ECSProcess(
	Twili &twili,
	const char *rtld,
	const char *npdm,
	uint64_t title_id) :
	MonitoredProcess(twili),
	title_id(title_id) {
	
	// build virtual exefs
	virtual_exefs.SetRtld(std::make_shared<fs::ActualFile>(fopen(rtld, "rb")));
	virtual_exefs.SetNpdm(std::make_shared<fs::ActualFile>(fopen(npdm, "rb")));
}

void ECSProcess::ChangeState(MonitoredProcess::State state) {
	MonitoredProcess::ChangeState(state);
	
	if(state == MonitoredProcess::State::Attached) {
		ecs_pending = false;
	}
	
	if(state == MonitoredProcess::State::Exited) {
		if(ecs_pending) {
			ResultCode::AssertOk(
				twili.services.ldr_shel.SendSyncRequest<65001>( // ClearExternalContentSource
					trn::ipc::InRaw<uint64_t>(title_id)));
		}
	}
}

bool ECSProcess::PrepareForLaunch() {
	if(files.size() > 1) {
		throw ResultError(TWILI_ERR_TOO_MANY_MODULES);
	} else if(files.empty()) {
		throw ResultError(TWILI_ERR_NO_MODULES);
	}
	virtual_exefs.SetMain(fs::NRONSOTransmutationFile::Create(files.front()));

	printf("installing ExternalContentSource\n");
	KObject session;
	ResultCode::AssertOk(
		twili.services.ldr_shel.SendSyncRequest<65000>( // SetExternalContentSource
			trn::ipc::InRaw<uint64_t>(title_id),
			trn::ipc::OutHandle<KObject, trn::ipc::move>(session)));
	printf("installed ExternalContentSource\n");
	ecs_pending = true;
	
	ResultCode::AssertOk(
		twili.server.AttachSession<fs::ProcessFileSystem::IFileSystem>(std::move(session), virtual_exefs));

	return true;
}

} // namespace process
} // namespace twili
