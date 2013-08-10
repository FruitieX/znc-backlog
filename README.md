znc-backlog
===========

znc-backlog is a ZNC module that makes it easy to request backlog. Its intended
use is for when you have just launched your IRC client and gotten a few lines of
backlog sent to you, but want to read more. Instead of having to deal with
shelling into the box where you run ZNC and manually sifting through the logs,
you can issue a short command in your IRC client to request any amount of the
most recent lines of log.

Dependencies
------------

Requires make and znc-buildmod to build (should come bundled with ZNC)

Compiling & Installing
----------------------

	git clone git://github.com/FruitieX/znc-backlog
	cd znc-backlog
	make
	cp backlog.so /path/to/znc/modules/

Usage
-----

Upon loading the module for the first time, you have to specify a path to your
log files with the LogPath command. (can also be specified by passing the
path as an argument when loading the module)

	LogPath /path/to/your/logs/$USER_$NETWORK_$WINDOW_*.log

**$USER** will be replaced with your ZNC username, **$NETWORK** with the current
network and **$WINDOW** with the requested channel/window name. The last found
asterisk character **'\*'** in the string is assumed to be a date, and the order in
which the files are read is determined by a simple alphabetical sort. (ie.
date format order must be year-month-date)

After the module is loaded and LogPath is set, you can request for logs with:

	/msg *backlog <window-name> <num-lines>

A handy alias for weechat:

	/alias bl msg *backlog $channel $1

Now you can:

	/bl 42

to request 42 lines from the current window or just:

	/bl

to request the default amount (150) of lines from the current window.

TODO
----
- Currently the module can only be loaded on a per-network basis.
- Autodetect LogPath if possible
- Optimize?
- Server-time for timestamp, http://ircv3.org/extensions/server-time-3.2
- make install rules (see kylef's plugins)
- CDir instead of opendir(), see FileUtils.h
- wiki page for the mod
- see how timestamps are created in buffer playback code
- investigate '\*' lines in XChat server window, maybe random space somewhere?
