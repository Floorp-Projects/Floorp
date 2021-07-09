Preferences
===========

There are a couple of preferences associated with the Gecko remote
protocol:


`marionette.debugging.clicktostart`
-----------------------------------

Delay server startup until a modal dialogue has been clicked to
allow time for user to set breakpoints in the [Browser Toolbox].

[Browser Toolbox]: https://developer.mozilla.org/en-US/docs/Tools/Browser_Toolbox


`marionette.log.level` (deprecated)
-----------------------------------

This preference used to control the verbosity of the Marionette specific logger.
Marionette is now using the shared Remote logger, please see `remote.log.level`
in the [Remote Agent Preferences] documentation.


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

[Remote Agent Preferences]: ../../remote/Prefs.html

