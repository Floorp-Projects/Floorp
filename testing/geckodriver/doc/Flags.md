<!-- markdownlint-disable MD033 -->
# Flags

## <code>-\\-allow-hosts <var>ALLOW_HOSTS</var>...</code>

Values of the `Host` header to allow for incoming requests.

By default the value of <var>HOST</var> is allowed. If `--allow-hosts`
is provided, exactly the given values will be permitted. For example
`--allow-host geckodriver.test webdriver.local` will allow requests
with `Host` set to `geckodriver.test` or `webdriver.local`.

Requests with `Host` set to an IP address are always allowed.

## <code>-\\-allow-origins <var>ALLOW_ORIGINS</var>...</code>

Values of the `Origin` header to allow for incoming requests.

`Origin` is set by web browsers for all `POST` requests, and most
other cross-origin requests. By default any request with an `Origin`
header is rejected to protect against malicious websites trying to
access geckodriver running on the local machine.

If `--allow-origins` is provided, web services running on the given
origin will be able to make requests to geckodriver. For example
`--allow-origins https://webdriver.test:8080` will allow a web-based
service on the origin with scheme `https`, hostname `webdriver.test`,
and port `8080` to access the geckodriver instance.

## <code>-\\-android-storage <var>ANDROID_STORAGE</var></code>

**Deprecation warning**: This argument is deprecated and planned to be removed
with the 0.31.0 release of geckodriver. As such it shouldn't be used with version
0.30.0 or later anymore. By default the automatic detection will now use the
external storage location, which is always readable and writeable.

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
    If the device is rooted `internal` is used, otherwise `app`.
 <tr>
  <td>app
  <td><p>Location: `/data/data/%androidPackage%/test_root`</p>
    Based on the `androidPackage` capability that is passed as part of
    `moz:firefoxOptions` when creating a new session. Commands that
    change data in the app's directory are executed using run-as. This requires
    that the installed app is debuggable.
 <tr>
  <td>internal
  <td><p>Location: `/data/local/tmp/test_root`</p>
    The device must be rooted since when the app runs, files that are created
    in the profile, which is owned by the app user, cannot be changed by the
    shell user. Commands will be executed via `su`.
 <tr>
  <td>sdcard
  <td><p>Location: `$EXTERNAL_STORAGE/Android/data/%androidPackage%/files/test_root`</p>
    This location is supported by all versions of Android whether if the device
    is rooted or not.
</table>

## <code>-b <var>BINARY</var></code> / <code>-\\-binary <var>BINARY</var></code>

Path to the Firefox binary to use.  By default geckodriver tries to
find and use the system installation of Firefox, but that behaviour
can be changed by using this option.  Note that the `binary`
capability of the `moz:firefoxOptions` object that is passed when
[creating a new session] will override this option.

On Linux systems it will use the first _firefox_ binary found
by searching the `PATH` environmental variable, which is roughly
equivalent to calling [whereis(1)] and extracting the second column:

```shell
% whereis firefox
firefox: /usr/bin/firefox /usr/local/firefox
```

On macOS, the binary is found by looking for the first _firefox_
binary in the same fashion as on Linux systems.  This means it is
possible to also use `PATH` to control where geckodriver should
find Firefox on macOS.  It will then look for _/Applications/Firefox.app_.

On Windows systems, geckodriver looks for the system Firefox by
scanning the Windows registry.

[creating a new session]: https://w3c.github.io/webdriver/#new-session
[whereis(1)]: http://www.manpagez.com/man/1/whereis/

## <code>-\\-connect-existing</code>

Connect geckodriver to an existing Firefox instance.  This means
geckodriver will abstain from the default of starting a new Firefox
session.

The existing Firefox instance must have [Marionette] enabled.
To enable the remote protocol in Firefox, you can pass the
`--marionette` flag.  Unless the `marionette.port` preference
has been user-set, Marionette will listen on port 2828.  So when
using `--connect-existing` it is likely you will also have to use
`--marionette-port` to set the correct port.

## <code>-\\-host <var>HOST</var></code>

Host to use for the WebDriver server.  Defaults to 127.0.0.1.

## <code>-\\-jsdebugger</code>

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

## <code>-\\-log <var>LEVEL</var></code>

Set the Gecko and geckodriver log level.  Possible values are `fatal`,
`error`, `warn`, `info`, `config`, `debug`, and `trace`.

## <code>-\\-log-no-truncate</code>

Disables truncation of long log lines.

## <code>-\\-marionette-host <var>HOST</var></code>

Selects the host for geckodriver’s connection to the [Marionette]
remote protocol. Defaults to 127.0.0.1.

## <code>-\\-marionette-port <var>PORT</var></code>

Selects the port for geckodriver’s connection to the [Marionette]
remote protocol.

In the default mode where geckodriver starts and manages the Firefox
process, it will pick a free port assigned by the system and set the
`marionette.port` preference in the profile.

When `--connect-existing` is used and the Firefox process is not
under geckodriver’s control, it will simply connect to <var>PORT</var>.

`--connect-existing`: #connect-existing

## <code>-p <var>PORT</var></code> / <code>-\\-port <var>PORT</var></code>

Port to use for the WebDriver server.  Defaults to 4444.

A helpful trick is that it is possible to bind to 0 to get the
system to atomically assign a free port.

## <code>-\\-profile-root <var>PROFILE_ROOT</var></code>

Path to the directory to use when creating temporary profiles. By
default this is the system temporary directory. Both geckodriver and
Firefox must have read-write access to this path.

This setting can be useful when Firefox is sandboxed from the host
filesystem such that it doesn't share the same system temporary
directory as geckodriver (e.g. when running Firefox inside a container
or packaged as a snap).

## <code>-v[v]</code>

Increases the logging verbosity by to debug level when passing
a single `-v`, or to trace level if `-vv` is passed.  This is
analogous to passing `--log debug` and `--log trace`, respectively.

## <code>-\\-websocket-port<var>PORT</var></code>

Port to use to connect to WebDriver BiDi. Defaults to 9222.

A helpful trick is that it is possible to bind to 0 to get the
system to atomically assign a free port.

[Marionette]: /testing/marionette/index.rst
