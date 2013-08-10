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
	bool inChan(const CString& Chan);

private:
	CString			m_sLogPath;
};

bool CBacklogMod::OnLoad(const CString& sArgs, CString& sMessage) {
	m_sLogPath = sArgs;

	if(m_sLogPath.empty()) {
		m_sLogPath = GetNV("LogPath");
		if(m_sLogPath.empty()) {
			// TODO: guess logpath?
			PutModule("LogPath is empty, set it with the LogPath command (help for more info)");
		}
	} else {
		SetNV("LogPath", m_sLogPath);
		PutModule("LogPath set to: " + m_sLogPath);
	}
	return true;
}

CBacklogMod::~CBacklogMod() {
}

void CBacklogMod::OnModCommand(const CString& sCommand) {
	if (sCommand.Token(0).Equals("help")) {
		// TODO: proper help text, look how AddHelpCommand() does it in other ZNC code
		PutModule("Usage:");
		PutModule("<window-name> [num-lines] (e.g. #foo 42)");
		PutModule("");
		PutModule("Commands:");
		PutModule("Help (print this text)");
		PutModule("LogPath <path> (use keywords $USER, $NETWORK, $WINDOW and an asterisk * for date)");
		return;
	}
	else if (sCommand.Token(0).Equals("logpath")) {
		if(sCommand.Token(1, true).empty()) {
			PutModule("Usage: LogPath <path> (use keywords $USER, $NETWORK, $WINDOW and an asterisk * for date:)");
			PutModule("Current LogPath is set to: " + GetNV("LogPath"));
			return;
		}

		m_sLogPath = sCommand.Token(1, true);
		SetNV("LogPath", m_sLogPath);
		PutModule("LogPath set to: " + m_sLogPath);
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
	reqLines = std::max(std::min(reqLines, 1000), 1);

	CString Path = m_sLogPath.substr(); // make copy
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

	bool isInChan = CBacklogMod::inChan(Channel);

	if(printedLines == 0) {
		PutModule("No log files found for window " + Channel + " in " + DirPath + "/");
		return;
	} else if (isInChan) {
		m_pNetwork->PutUser(":***!znc@znc.in PRIVMSG " + Channel + " :" + "Backlog playback...", GetClient());
	} else {
		PutModule("*** Backlog playback...");
	}

	// now actually print
	for (std::vector<CString>::reverse_iterator it = LinesToPrint.rbegin(); it != LinesToPrint.rend(); ++it) {
		 if(isInChan) {
			CString Line = *it;
			size_t FirstSpace = Line.find_first_of(' ');
			size_t Len = Line.find_first_of(' ', FirstSpace + 1) - FirstSpace;
			CString Nick = Line.substr(FirstSpace + 2, Len - 3);

			m_pNetwork->PutUser(":" + Nick + "!znc@znc.in PRIVMSG " + Channel + " :" + Line.substr(0, FirstSpace) + Line.substr(FirstSpace + Len, Line.npos), GetClient());
		 } else {
			PutModule(*it);
		 }
	}

	if (isInChan) {
		m_pNetwork->PutUser(":***!znc@znc.in PRIVMSG " + Channel + " :" + "Playback complete.", GetClient());
	} else {
		PutModule("*** Playback complete.");
	}
}

bool CBacklogMod::inChan(const CString& Chan) {
	const std::vector <CChan*>& vChans (m_pNetwork->GetChans());

	for (std::vector<CChan*>::const_iterator it = vChans.begin(); it != vChans.end(); ++it) {
		CChan *curChan = *it;
		if(Chan.StrCmp(curChan->GetName()) == 0) {
			return true;
		}
	}

	return false;
}



template<> void TModInfo<CBacklogMod>(CModInfo& Info) {
	Info.AddType(CModInfo::NetworkModule);
	Info.AddType(CModInfo::GlobalModule);
	Info.SetWikiPage("backlog");
	Info.SetArgsHelpText("Takes as optional argument (and if given, sets) the log path, use keywords: $USER, $NETWORK and $WINDOW");
	Info.SetHasArgs(true);
}

NETWORKMODULEDEFS(CBacklogMod, "Module for getting the last X lines of a channels log.")
