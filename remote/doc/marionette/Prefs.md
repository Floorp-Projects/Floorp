# Preferences

There are a couple of [Remote Agent preferences] associated with the Gecko remote
protocol. Those listed below are additional ones uniquely used for Marionette.

[Remote Agent preferences]: /remote/Prefs.md

## `marionette.debugging.clicktostart`

Delay server startup until a modal dialogue has been clicked to
allow time for user to set breakpoints in the [Browser Toolbox].

[Browser Toolbox]: /devtools-user/browser_toolbox/index.rst

## `marionette.port`

Defines the port on which the Marionette server will listen.  Defaults
to port 2828.

This can be set to 0 to have the system atomically allocate a free
port, which can be useful when running multiple Marionette servers
on the same system.  The effective port is written to the user
preference file when the server has started and is also logged to
stdout.
