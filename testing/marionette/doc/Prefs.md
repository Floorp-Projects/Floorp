Preferences
===========

There are a couple of preferences associated with the Gecko remote
protocol:


`marionette.enabled`
--------------------

Starts and stops the Marionette server.  This will cause a TCP
server to bind to the port defined by `marionette.port`.

If Gecko has not been started with the `-marionette` flag or the
`MOZ_MARIONETTE` environment variable, changing this preference
will have no effect.  For Marionette to be enabled, either one of
these options _must_ be given to Firefox or Fennec for Marionette
to start.


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

The available levels are, in descending order of severity, `trace`,
`debug`, `config`, `info`, `warn`, `error`, and `fatal`.  The value
is treated case-insensitively.


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


`marionette.contentListener`
----------------------------

Used internally in Marionette for determining whether content scripts
can safely be reused.  Should not be tweaked manually.

This preference is scheduled for removal.
