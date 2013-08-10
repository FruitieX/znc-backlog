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
LogPath as an argument when loading)

	LogPath /path/to/your/logs/$USER\_$NETWORK\_$WINDOW\_\*.log

$USER will be replaced with your ZNC username, $NETWORK with the current
network and $WINDOW with the requested channel/window name.

After the module is loaded and LogPath is set, you can request for logs with:

	/msg \*backlog \<window-name\> \<num-lines\>


Currently the module can only be loaded on a per-network basis.
