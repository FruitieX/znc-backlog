/*
 * Copyright (C) 2004-2013 ZNC, see the NOTICE file for details.
 * Copyright (C) 2013 Rasmus Eskola <fruitiex@gmail.com>
 * based on sample.cpp sample module code
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
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/Modules.h>
#include <dirent.h>
#include <vector>
#include <algorithm>

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
	LogPath = sArgs;

	if(LogPath.empty()) {
		LogPath = GetNV("LogPath");
		if(LogPath.empty()) {
			// TODO: guess logpath?
			PutModule("LogPath is empty, set it with the LogPath command");
			PutModule("Usage: LogPath <path> (use keywords $USER, $NETWORK, $WINDOW)");
		}
	} else {
		SetNV("LogPath", LogPath);
		PutModule("LogPath set to: " + LogPath);
	}
	return true;
}

CBacklogMod::~CBacklogMod() {
}

void CBacklogMod::OnModCommand(const CString& sCommand) {
	if (sCommand.Token(0).CaseCmp("help") == 0) {
		// TODO: proper help text, look how AddHelpCommand() does it in other ZNC code
		PutModule("Help");
		return;
	}
	else if (sCommand.Token(0).CaseCmp("logpath") == 0) {
		LogPath = sCommand.Token(1, true);

		if(LogPath.empty()) {
			PutModule("Usage: LogPath <path> (use keywords $USER, $NETWORK, $WINDOW)");
			PutModule("Current LogPath is set to: " + GetNV("LogPath"));
			return;
		}

		SetNV("LogPath", LogPath);
		PutModule("LogPath set to: " + LogPath);
		return;
	}

	// TODO: handle these differently depending on how the module was loaded
	CString User = (m_pUser ? m_pUser->GetUserName() : "UNKNOWN");
	CString Network = (m_pNetwork ? m_pNetwork->GetName() : "znc");
	CString Channel = sCommand.Token(0);

	int printedLines = 0;
	int reqLines = sCommand.Token(1).ToInt();
	if(reqLines <= 0) {
		reqLines = 150;
	}

	CString Path = LogPath.substr(); // make copy
	Path.Replace("$NETWORK", Network);
	Path.Replace("$WINDOW", Channel);
	Path.Replace("$USER", User);

	CString DirPath = Path.substr(0, Path.find_last_of("/"));
	CString FilePath;

	std::vector<CString> FileList;
	std::vector<CString> LinesToPrint;

	// gather list of all log files for requested channel/window
	DIR *dir;
	struct dirent *ent;
	if ((dir = opendir (DirPath.c_str())) != NULL) {
		while ((ent = readdir (dir)) != NULL) {
			FilePath = DirPath + "/" + ent->d_name;
			//PutModule("DEBUG: " + FilePath + " " + Path);
			if(FilePath.StrCmp(Path, Path.find_last_of("*")) == 0) {
				FileList.push_back(FilePath);
			}
		}
		closedir (dir);
	} else {
		PutModule("Could not list directory " + DirPath + ": " + strerror(errno));
		return;
	}

	std::sort(FileList.begin(), FileList.end());

	// loop through list of log files one by one starting from most recent...
	for (std::vector<CString>::reverse_iterator it = FileList.rbegin(); it != FileList.rend(); ++it) {
		CFile LogFile(*it);
		CString Line;
		std::vector<CString> Lines;

		if (LogFile.Open()) {
			while (LogFile.ReadLine(Line)) {
				// store lines from file into Lines
				Lines.push_back(Line);
			}
		} else {
			PutModule("Could not open log file [" + sCommand + "]: " + strerror(errno));
			continue;
		}

		LogFile.Close();

		// loop through Lines in reverse order, push to LinesToPrint
		for (std::vector<CString>::reverse_iterator itl = Lines.rbegin(); itl != Lines.rend(); ++itl) {
			LinesToPrint.push_back(*itl);
			printedLines++;

			if(printedLines >= reqLines) {
				break;
			}
		}

		if(printedLines >= reqLines) {
			break;
		}
	}

	// now actually print
	for (std::vector<CString>::reverse_iterator it = LinesToPrint.rbegin(); it != LinesToPrint.rend(); ++it) {
		PutModule(*it);
	}
}

template<> void TModInfo<CBacklogMod>(CModInfo& Info) {
	Info.AddType(CModInfo::NetworkModule);
	Info.AddType(CModInfo::GlobalModule);
	Info.SetWikiPage("backlog");
	Info.SetArgsHelpText("Takes as optional argument (and if given, sets) the log path, use keywords: $USER, $NETWORK and $WINDOW");
	Info.SetHasArgs(true);
}

NETWORKMODULEDEFS(CBacklogMod, "Module for getting the last X lines of a channels log.")
