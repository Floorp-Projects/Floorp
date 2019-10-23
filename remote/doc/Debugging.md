Debugging
=========

Increasing the logging verbosity
--------------------------------

To increase the internal logging verbosity you can use the
`remote.log.level` [preference].

If you use mach to start Firefox:

	./mach run --setpref "remote.enabled=true" --setpref "remote.log.level=Debug" --remote-debugger

Enabling logging of emitted events
----------------------------------

To dump events produced by EventEmitter,
including CDP events produced by the remote agent,
you can use the `toolkit.dump.emit` [preference]:

    ./mach run --setpref "remote.enabled=true" --setpref "toolkit.dump.emit=true" --remote-debugger

[preference]: ./Prefs.html
