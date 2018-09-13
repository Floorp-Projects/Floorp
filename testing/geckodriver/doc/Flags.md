Flags
=====

#### <code>-b <var>BINARY</var></code>/<code>--binary <var>BINARY</var></code>

Path to the Firefox binary to use.  By default geckodriver tries to
find and use the system installation of Firefox, but that behaviour
can be changed by using this option.  Note that the `binary`
capability of the `moz:firefoxOptions` object that is passed when
[creating a new session] will override this option.

On Linux systems it will use the first _firefox_ binary found
by searching the `PATH` environmental variable, which is roughly
equivalent to calling [whereis(1)] and extracting the second column:

	% whereis firefox
	firefox: /usr/bin/firefox /usr/local/firefox

On macOS, the binary is found by looking for the first _firefox-bin_
binary in the same fashion as on Linux systems.  This means it is
possible to also use `PATH` to control where geckodriver should
find Firefox on macOS.  It will then look for _/Applications/Firefox.app_.

On Windows systems, geckodriver looks for the system Firefox by
scanning the Windows registry.

[creating a new session]: https://w3c.github.io/webdriver/#new-session
[whereis(1)]: http://www.manpagez.com/man/1/whereis/


#### `--connect-existing`

Connect geckodriver to an existing Firefox instance.  This means
geckodriver will abstain from the default of starting a new Firefox
session.

The existing Firefox instance must have [Marionette] enabled.
To enable the remote protocol in Firefox, you can pass the
`-marionette` flag.  Unless the `marionette.port` preference
has been user-set, Marionette will listen on port 2828.  So when
using `--connect-existing` it is likely you will also have to use
[`--marionette-port`] to set the correct port.

[`--marionette-port`]: #marionette-port


#### <code>--host <var>HOST</var></code>

Host to use for the WebDriver server.  Defaults to 127.0.0.1.


#### <code>--log <var>LEVEL</var></code>

Set the Gecko and geckodriver log level.  Possible values are `fatal`,
`error`, `warn`, `info`, `config`, `debug`, and `trace`.


#### <code>--marionette-port <var>PORT</var></code>

Selects the port for geckodriver’s connection to the [Marionette]
remote protocol.

In the default mode where geckodriver starts and manages the Firefox
process, it will pick a free port assigned by the system and set the
`marionette.port` preference in the profile.

When [`--connect-existing`] is used and the Firefox process is not
under geckodriver’s control, it will simply connect to <var>PORT</var>.

[`--connect-existing`]: #connect-existing


#### <code>-p <var>PORT</var></code>/<code>--port <var>PORT</var></code>

Port to use for the WebDriver server.  Defaults to 4444.

A helpful trick is that it is possible to bind to 0 to get the
system to atomically assign a free port.


#### <code>--jsdebugger</code>

Attach [browser toolbox] debugger when Firefox starts.  This is
useful for debugging [Marionette] internals.

[browser toolbox]: https://developer.mozilla.org/en-US/docs/Tools/Browser_Toolbox


#### <code>-v<var>[v]</var></code>

Increases the logging verbosity by to debug level when passing
a single `-v`, or to trace level if `-vv` is passed.  This is
analogous to passing `--log debug` and `--log trace`, respectively.
