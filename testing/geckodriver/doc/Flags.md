Flags
=====

#### <code>&#x2D;&#x2D;android-storage <var>ANDROID_STORAGE</var></code>

Selects the test data location on the Android device, eg. the Firefox profile.
By default `auto` is used.

<style type="text/css">
  table { width: 100%; margin-bottom: 2em; }
  table, th, td { border: solid gray 1px; }
  td, th { padding: 10px; text-align: left; vertical-align: middle; }
  td:nth-child(1), th:nth-child(1) { width: 10em; text-align: center; }
</style>

<table>
 <thead>
  <tr>
    <th>Value
    <th>Description
  </tr>
 </thead>

 <tr>
  <td>auto
  <td>Best suitable location based on whether the device is rooted.<br/>
    If the device is rooted <code>internal</code> is used, otherwise <code>app</code>.
 <tr>
  <td>app
  <td><p>Location: <code>/data/data/%androidPackage%/test_root</code></p>
    Based on the <code>androidPackage</code> capability that is passed as part of
    <code>moz:firefoxOptions</code> when creating a new session. Commands that
    change data in the app's directory are executed using run-as. This requires
    that the installed app is debuggable.
 <tr>
  <td>internal
  <td><p>Location: <code>/data/local/tmp/test_root</code></p>
    The device must be rooted since when the app runs, files that are created
    in the profile, which is owned by the app user, cannot be changed by the
    shell user. Commands will be executed via <code>su</code>.
 <tr>
  <td>sdcard
  <td><p>Location: <code>/mnt/sdcard/test_root</code></p>
    This location is not supported on Android 11+ due to the
    <a href="https://developer.android.com/about/versions/11/privacy/storage">
    changes related to scoped storage</a>.
</table>


#### <code>-b <var>BINARY</var></code> / <code>&#x2D;&#x2D;binary <var>BINARY</var></code>

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


#### <code>&#x2D;&#x2D;connect-existing</code>

Connect geckodriver to an existing Firefox instance.  This means
geckodriver will abstain from the default of starting a new Firefox
session.

The existing Firefox instance must have [Marionette] enabled.
To enable the remote protocol in Firefox, you can pass the
`-marionette` flag.  Unless the `marionette.port` preference
has been user-set, Marionette will listen on port 2828.  So when
using `--connect-existing` it is likely you will also have to use
`--marionette-port` to set the correct port.

[`&#x2D;&#x2D;marionette-port`]: #marionette-port


#### <code>&#x2D;&#x2D;host <var>HOST</var></code>

Host to use for the WebDriver server.  Defaults to 127.0.0.1.


#### <code>&#x2D;&#x2D;log <var>LEVEL</var></code>

Set the Gecko and geckodriver log level.  Possible values are `fatal`,
`error`, `warn`, `info`, `config`, `debug`, and `trace`.


#### <code>&#x2D;&#x2D;marionette-host <var>HOST</var></code>

Selects the host for geckodriver’s connection to the [Marionette]
remote protocol. Defaults to 127.0.0.1.


#### <code>&#x2D;&#x2D;marionette-port <var>PORT</var></code>

Selects the port for geckodriver’s connection to the [Marionette]
remote protocol.

In the default mode where geckodriver starts and manages the Firefox
process, it will pick a free port assigned by the system and set the
`marionette.port` preference in the profile.

When `--connect-existing` is used and the Firefox process is not
under geckodriver’s control, it will simply connect to <var>PORT</var>.

[`--connect-existing`]: #connect-existing


#### <code>-p <var>PORT</var></code> / <code>&#x2D;&#x2D;port <var>PORT</var></code>

Port to use for the WebDriver server.  Defaults to 4444.

A helpful trick is that it is possible to bind to 0 to get the
system to atomically assign a free port.


#### <code>&#x2D;&#x2D;jsdebugger</code>

Attach [browser toolbox] debugger when Firefox starts.  This is
useful for debugging [Marionette] internals.

To be prompted at the start of the test run or between tests,
you can set the `marionette.debugging.clicktostart` preference to
`true`.

For reference, below is the list of preferences that enables the
chrome debugger. These are all set implicitly when the
argument is passed to geckodriver.

  * `devtools.browsertoolbox.panel` -> `jsdebugger`

    Selects the Debugger panel by default.

  * `devtools.chrome.enabled` → true

    Enables debugging of chrome code.

  * `devtools.debugger.prompt-connection` → false

    Controls the remote connection prompt.  Note that this will
    automatically expose your Firefox instance to localhost.

  * `devtools.debugger.remote-enabled` → true

    Allows a remote debugger to connect, which is necessary for
    debugging chrome code.


[browser toolbox]: https://developer.mozilla.org/en-US/docs/Tools/Browser_Toolbox


#### <code>-v<var>[v]</var></code>

Increases the logging verbosity by to debug level when passing
a single `-v`, or to trace level if `-vv` is passed.  This is
analogous to passing `--log debug` and `--log trace`, respectively.
