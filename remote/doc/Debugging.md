# Debugging

For other debugging resources, see also: Remote project [wiki]

## Increasing the logging verbosity

To increase the internal logging verbosity you can use the
`remote.log.level` [preference].

If you use mach to start Firefox:

```shell
% ./mach run --setpref "remote.log.level=Trace" --remote-debugging-port
```

By default, long log lines are truncated. To print long lines in full, you
can set `remote.log.truncate` to false.

## Enabling logging of emitted events

To dump events produced by EventEmitter,
including CDP events produced by the Remote Agent,
you can use the `toolkit.dump.emit` [preference]:

```shell
% ./mach run --setpref "toolkit.dump.emit=true" --remote-debugging-port
```

## Logging observer notifications

Observer notifications are used extensively throughout the
code and it can sometimes be useful to log these to see what is
available and when they are fired.

The `MOZ_LOG` environment variable controls the C++ logs and takes
the name of the subsystem along with a verbosity setting.  See
[prlog.h] for more details.

```shell
MOZ_LOG=ObserverService:5
```

You can optionally redirect logs away from stdout to a file:

```shell
MOZ_LOG_FILE=service.log
```

This enables `LogLevel::Debug` level information and places all
output in the file service.log in your current working directory.

[preference]: Prefs.md
[prlog.h]: https://searchfox.org/mozilla-central/source/nsprpub/pr/include/prlog.h
[wiki]: https://wiki.mozilla.org/Remote/Developer_Resources
