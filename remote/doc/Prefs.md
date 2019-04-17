Preferences
===========

There are a couple of preferences associated with the remote agent:


Configurable preferences
------------------------

### `remote.enabled`

Indicates whether the remote agent is enabled.  When the remote
agent is enabled, it exposes a [`--remote-debugger` flag] for Firefox.
When set to false, the remote agent will not be loaded on startup.

[`--remote-debugger` flag]: Usage.html

### `remote.force-local`

Limits the remote agent to be allowed to listen on loopback devices,
e.g. 127.0.0.1, localhost, and ::1.

### `remote.log.level`

Defines the verbosity of the internal logger.  Available levels
are, in descending order of severity, `Trace`, `Debug`, `Config`,
`Info`, `Warn`, `Error`, and `Fatal`.  Note that the value is
treated case-sensitively.
