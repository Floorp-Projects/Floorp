Preferences
===========

There are a couple of preferences associated with the Remote Agent:


Configurable preferences
------------------------

### `remote.force-local`

Limits the Remote Agent to be allowed to listen on loopback devices,
e.g. 127.0.0.1, localhost, and ::1.

### `remote.log.level`

Defines the verbosity of the internal logger.  Available levels
are, in descending order of severity, `Trace`, `Debug`, `Config`,
`Info`, `Warn`, `Error`, and `Fatal`.  Note that the value is
treated case-sensitively.

### `remote.log.truncate`

Defines whether long log messages should be truncated. Defaults to true.
