Preferences
===========

There are a couple of preferences associated with the Gecko remote
protocol:


`marionette.debugging.clicktostart`
-----------------------------------

Delay server startup until a modal dialogue has been clicked to
allow time for user to set breakpoints in the [Browser Toolbox].

[Browser Toolbox]: https://developer.mozilla.org/en-US/docs/Tools/Browser_Toolbox


`marionette.log.level`
----------------------

Sets the verbosity level of the Marionette logger repository.  Note
that this preference does not control the verbosity of other loggers
used in Firefox or Fennec.

The available levels are, in descending order of severity, `Trace`,
`debug`, `config`, `info`, `warn`, `error`, and `fatal`.  The value
is treated case-insensitively.


`marionette.log.truncate`
-------------------------

Certain log messages that are known to be long, such as wire protocol
dumps, are truncated.  This preference causes them not to be truncated.


`marionette.port`
-----------------

Defines the port on which the Marionette server will listen.  Defaults
to port 2828.

This can be set to 0 to have the system atomically allocate a free
port, which can be useful when running multiple Marionette servers
on the same system.  The effective port is written to the user
preference file when the server has started and is also logged to
stdout.


`marionette.prefs.recommended`
------------------------------

By default Marionette attempts to set a range of preferences deemed
suitable in automation when it starts.  These include the likes of
disabling auto-updates, Telemetry, and first-run UX.

The user preference file takes presedence over the recommended
preferences, meaning any user-defined preference value will not be
overridden.
