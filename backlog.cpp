/*
 * Copyright (C) 2004-2013 ZNC, see the NOTICE file for details.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <znc/FileUtils.h>
#include <znc/Client.h>
#include <znc/Chan.h>
#include <znc/Modules.h>

class CBacklogMod : public CModule {
public:
	MODCONSTRUCTOR(CBacklogMod) {}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage);
	virtual ~CBacklogMod();
	virtual void OnModCommand(const CString& sCommand);

private:
	CString			LogPath;
};

bool CBacklogMod::OnLoad(const CString& sArgs, CString& sMessage) {
	PutModule("I'm being loaded with the arguments: [" + sArgs + "]");
	return true;
}

CBacklogMod::~CBacklogMod() {
	PutModule("I'm being unloaded!");
}

void CBacklogMod::OnModCommand(const CString& sCommand) {
	CFile LogFile(sCommand);
	CString Line;

	if (LogFile.Open()) {
		while (LogFile.ReadLine(Line)) {
			PutModule(Line);
		}
	} else {
		PutModule("Could not open log file [" + sCommand + "]: " + strerror(errno));
	}

	LogFile.Close();
}

template<> void TModInfo<CBacklogMod>(CModInfo& Info) {
	Info.AddType(CModInfo::NetworkModule);
	Info.AddType(CModInfo::GlobalModule);
	Info.SetWikiPage("backlog");
	Info.SetArgsHelpText("Takes path to logs as argument, use keywords: $USER, $NETWORK and $WINDOW");
	Info.SetHasArgs(true);
}

NETWORKMODULEDEFS(CBacklogMod, "Module for getting the last X lines of a channels log.")
