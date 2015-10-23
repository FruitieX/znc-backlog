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
};

bool CBacklogMod::OnLoad(const CString& sArgs, CString& sMessage) {
    CString LogPath = sArgs;

    if(LogPath.empty()) {
        LogPath = GetNV("LogPath");
        if(LogPath.empty()) {
            // TODO: guess logpath?
            PutModule("LogPath is empty, set it with the LogPath command (help for more info)");
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
    CString Arg = sCommand.Token(1);
    if (sCommand.Token(0).Equals("help")) {
        // TODO: proper help text, look how AddHelpCommand() does it in other ZNC code
        PutModule("Usage:");
        PutModule("<window-name> [num-lines] (e.g. #foo 42)");
        PutModule("");
        PutModule("Commands:");
        PutModule("Help (print this text)");
        PutModule("LogPath <path> (use keywords $USER, $NETWORK, $WINDOW and an asterisk * for date)");
        PutModule("PrintStatusMsgs true/false (print join/part/rename messages)");
        return;
    } else if (sCommand.Token(0).Equals("logpath")) {
        if(Arg.empty()) {
            PutModule("Usage: LogPath <path> (use keywords $USER, $NETWORK, $WINDOW and an asterisk * for date:)");
            PutModule("Current LogPath is set to: " + GetNV("LogPath"));
            return;
        }

        CString LogPath = sCommand.Token(1, true);
        SetNV("LogPath", LogPath);
        PutModule("LogPath set to: " + LogPath);
        return;
    } else if (sCommand.Token(0).Equals("PrintStatusMsgs")) {
        if(Arg.empty() || (!Arg.Equals("true", true) && !Arg.Equals("false", true))) {
            PutModule("Usage: PrintStatusMsgs true/false");
            return;
        }

        SetNV("PrintStatusMsgs", Arg);
        PutModule("PrintStatusMsgs set to: " + Arg);
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

    CString Path = GetNV("LogPath").substr(); // make copy
    Path.Replace("$NETWORK", Network);
    Path.Replace("$WINDOW", CString(Channel.Replace_n("/", "-").Replace_n("\\", "-")).AsLower());
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
            if(FilePath.AsLower().StrCmp(Path.AsLower(), Path.find_last_of("*")) == 0) {
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

        if (LogFile.GetShortName() == "." || LogFile.GetShortName() == "..") {
            continue;
        }

        if (LogFile.Open()) {
            Lines.push_back("[00:00:00] <***> File: " + LogFile.GetShortName()); //Parse name to get date prettier?
            printedLines--;
            while (LogFile.ReadLine(Line)) {
                try {
                    // is line a part/join/rename etc message (nick ***), do we want to print it?
                    // find nick by finding first whitespace, then moving one char right
                    if(Line.at(Line.find_first_of(' ') + 1) == '*' && !GetNV("PrintStatusMsgs").ToBool()) {
                        continue;
                    }

                    Lines.push_back(Line);
                } catch (int e) {
                    // malformed log line, ignore
                    PutModule("Malformed log line found in " + *it + ": " + Line);
                }
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

    if(printedLines <= 0) {
        m_pNetwork->PutUser(":***!znc@znc.in PRIVMSG " + Channel + " :No log files found for window " + Channel + " in " + DirPath + "/", GetClient());
        return;
    } else {
        //RFC prefixes: # ! & +
        if (Channel.StartsWith("#") || Channel.StartsWith("&") ||
            Channel.StartsWith("!") || Channel.StartsWith("+")) { //Normal channel
            m_pNetwork->PutUser(":***!znc@znc.in PRIVMSG " + Channel + " :Backlog playback...", GetClient());
        } else {
            m_pNetwork->PutUser(":" + Channel + "!znc@znc.in PRIVMSG " + Channel + " :<***> Backlog playback...", GetClient());
        }
    }

    // now actually print
    for (std::vector<CString>::reverse_iterator it = LinesToPrint.rbegin(); it != LinesToPrint.rend(); ++it) {
        CString Line = *it;
        size_t FirstWS = Line.find_first_of(' '); // position of first whitespace char in line
        size_t NickLen = 3;
        CString Nick = "***";
        CString MessageType = "PRIVMSG ";

        try {
            // a log line looks like: [HH:MM:SS] <Nick> Message
            // < and > are illegal characters in nicknames, so we can
            // search for these
            if(Line.at(FirstWS + 1) == '<') { // normal message
                // nicklen includes surrounding < >
                NickLen = Line.find_first_of('>') - Line.find_first_of('<') + 1;
                // but we don't want them in Nick so subtract by two
                Nick = Line.substr(FirstWS + 2, NickLen - 2);
            }
            else if(Line.at(FirstWS + 1) == '-') { // IRC notice
                // nicklen includes surrounding - -
                NickLen = Line.find_first_of('-', FirstWS + 2) - Line.find_first_of('-') + 1;
                // but we don't want them in Nick so subtract by two
                Nick = Line.substr(FirstWS + 2, NickLen - 2);
                MessageType = "NOTICE ";
            }

            //RFC prefixes: # ! & +
            if (Channel.StartsWith("#") || Channel.StartsWith("&") ||
                Channel.StartsWith("!") || Channel.StartsWith("+")) { //Normal channel
                m_pNetwork->PutUser(":" + Nick + "!znc@znc.in " + MessageType + Channel + " :" + Line.substr(0, FirstWS) + Line.substr(FirstWS + NickLen + 1, Line.npos), GetClient());
            } else { //Privmsg
                m_pNetwork->PutUser(":" + Channel + "!znc@znc.in " + MessageType + Channel + " :" + Line.substr(0, FirstWS)  + " <" + Nick + ">" + Line.substr(FirstWS + NickLen + 1, Line.npos), GetClient());
            }       

            
        } catch (int e) {
            PutModule("Malformed log line! " + Line);
        }
    }

    if (Channel.StartsWith("#") || Channel.StartsWith("&") ||
        Channel.StartsWith("!") || Channel.StartsWith("+")) { //Normal channel
        m_pNetwork->PutUser(":***!znc@znc.in PRIVMSG " + Channel + " :" + "Playback Complete.", GetClient());
    } else {
        m_pNetwork->PutUser(":" + Channel + "!znc@znc.in PRIVMSG " + Channel + " :<***> " + "Playback Complete.", GetClient());
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
