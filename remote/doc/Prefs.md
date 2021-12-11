Preferences
===========

There are a couple of preferences associated with the Remote Agent:


Configurable preferences
------------------------

### `remote.active-protocols`

Defines the remote protocols that are active. Available protocols are,
WebDriver BiDi (`1`), and CDP (`2`). Multiple protocols can be activated
at the same time by using bitwise or with the values. Defaults to `3`
in Nightly builds, and `2` otherwise.

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

### `remote.prefs.recommended`

By default remote protocols attempts to set a range of preferences deemed
suitable in automation when it starts.  These include the likes of
disabling auto-updates, Telemetry, and first-run UX. Set this preference to
`false` to skip setting those preferences, which is mostly useful for internal
Firefox CI suites.

The user preference file takes precedence over the recommended
preferences, meaning any user-defined preference value will not be
overridden.
