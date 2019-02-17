Preferences
===========

There are a couple of preferences associated with the remote agent:


Configurable preferences
------------------------

### `remote.enabled`

Indicates whether the remote agent is enabled.  When the remote
agent is enabled, it exposes a `--remote-debugger` flag for Firefox.
When set to false, the remote agent will not be loaded on startup.

### `remote.force-local`

Limits the remote agent to be allowed to listen on loopback devices,
e.g. 127.0.0.1, localhost, and ::1.

### `remote.log.level`

Defines the verbosity of the internal logger.  Available levels
are, in descending order of severity, `Trace`, `Debug`, `Config`,
`Info`, `Warn`, `Error`, and `Fatal`.  Note that the value is
treated case-sensitively.


Informative preferences
-----------------------

The following preferences are set when the remote agent starts the
HTTPD frontend:

### `remote.httpd.scheme`

Scheme the server is listening on, e.g. `httpd`.

### `remote.httpd.host`

Hostname the server is bound to.

### `remote.httpd.port`

The port bound by the server.  When starting Firefox with
`--remote-debugger` you can ask the remote agent to listen on port
0 to have the system atomically allocate a free port.  You can then
later check this preference to find out on what port it is listening:

	./firefox --remote-debugger :0
	1548002326113	RemoteAgent	INFO	Remote debugging agent listening on http://localhost:16738/
