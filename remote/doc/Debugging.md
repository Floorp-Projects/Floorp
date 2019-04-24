Debugging
=========

Increasing the logging verbosity
--------------------------------

To increase the internal logging verbosity you can use the
`remote.log.level` [preference].

If you use mach to start the Firefox:

	./mach run --setpref "remote.enabled=true" --setpref "remote.log.level=Debug" --remote-debugger

[preference]: ./Prefs.md
