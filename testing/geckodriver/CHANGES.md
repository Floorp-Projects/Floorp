Change log
==========

All notable changes to this program are documented in this file.

0.29.1  (2021-04-09), `970ef713fe58`)
-------------------------------------

### Known problems

- _macOS 10.15 (Catalina):_

  Due to the requirement from Apple that all programs must be
  notarized, geckodriver will not work on Catalina if you manually
  download it through another notarized program, such as Firefox.

  Whilst we are working on a repackaging fix for this problem, you can
  find more details on how to work around this issue in the [macOS
  notarization] section of the documentation.

- _Android:_

  Marionette will only be enabled in GeckoView based applications when the
  Firefox preference `devtools.debugger.remote-enabled` is set to `True` via
  [`moz:firefoxOptions`]. This will be fixed in the Firefox 90 release for
  Android.

### Added

- When testing GeckoView based applications on Android it's now enough to
  specify the `androidPackage` capability. The appropriate activity name,
  and required intent arguments will now automatically be used for
  applications released by Mozilla.

- Native AArch64 (M1) builds of geckodriver for MacOS are now available. These
  are currently shipped as Tier2 due to missing test infrastructure. Please let
  us know if you experience issues.

### Fixed

- Fixed a stack overflow crash in thread 'webdriver dispatcher' when
  handling certain device errors.

- Fixed an application crash due to missing permissions on unrooted devices
  by changing the location of the test related files, e.g the profile folder.
  Therefore the deprecated &#x2D;&#x2D;android-storage command line argument
  now defaults to the `sdcard` option, which changed its location to
  `$EXTERNAL_STORAGE/Android/data/%androidPackage%/files/`. With this change
  proper support for unrooted devices running Android 10+ has been added.

  _Note_: Do not use the &#x2D;&#x2D;android-storage command line argument
  anymore unless there is a strong reason. It will be removed in a future
  release.


0.29.0  (2021-01-14, `cf6956a5ec8e`)
------------------------------------

### Known problems

- _macOS 10.15 (Catalina):_

  Due to the requirement from Apple that all programs must be
  notarized, geckodriver will not work on Catalina if you manually
  download it through another notarized program, such as Firefox.

  Whilst we are working on a repackaging fix for this problem, you can
  find more details on how to work around this issue in the [macOS
  notarization] section of the documentation.

- _Android:_

  Marionette will only be enabled in GeckoView based applications when the
  Firefox preference `devtools.debugger.remote-enabled` is set to `True` via
  [`moz:firefoxOptions`]. This will be fixed in one of the upcoming Firefox
  for Android releases.

  In some cases geckodriver could crash due to a stack overflow when handling
  certain device errors.

  On unrooted Android 10+ devices startup crashes of the application can be
  experienced due to an inappropriate location of test related files, e.g the
  profile folder.

### Added

- Introduced the new boolean capability `moz:debuggerAddress` that can be used
  to opt-in to the experimental Chrome DevTools Protocol (CDP) implementation.
  A string capability with the same name will be returned by [`NewSession`],
  which contains the `host:port` combination of the HTTP server that can be
  used to query for websockets of available targets.

  Note: For this experimental feature the site-isolation support of
  Firefox aka [Fission] will be not available.

0.28.0  (2020-11-03, `c00d2b6acd3f`)
------------------------------------

### Known problems

- _macOS 10.15 (Catalina):_

  Due to the requirement from Apple that all programs must be
  notarized, geckodriver will not work on Catalina if you manually
  download it through another notarized program, such as Firefox.

  Whilst we are working on a repackaging fix for this problem, you can
  find more details on how to work around this issue in the [macOS
  notarization] section of the documentation.

- _Android:_

  Marionette will only be enabled in GeckoView based applications when the
  Firefox preference `devtools.debugger.remote-enabled` is set to `True` via
  [`moz:firefoxOptions`]. This will be fixed in one of the upcoming Firefox
  for Android releases.

  In some cases geckodriver could crash due to a stack overflow when handling
  certain device errors.

  On unrooted Android 10+ devices startup crashes of the application can be
  experienced due to an inappropriate location of test related files, e.g the
  profile folder.

### Added

- The command line flag `--android-storage` has been added, to allow geckodriver
  to also control Firefox on root-less Android devices.
  See the [documentation][Flags] for available values.

### Fixed

- Firefox can be started again via a shell script that is located outside of the
  Firefox directory on Linux.

- If Firefox cannot be started by geckodriver the real underlying error message is
  now being reported.

- Version numbers for minor and extended support releases of Firefox are now parsed correctly.

### Removed

- Since Firefox 72 extension commands for finding an element’s anonymous children
  and querying its attributes are no longer needed, and have been removed.

0.27.0  (2020-07-27, `7b8c4f32cdde`)
------------------------------------

### Security Fixes

- CVE-2020-15660

  - Added additional checks on the `Content-Type` header for `POST`
    requests to disallow `application/x-www-form-urlencoded`,
    `multipart/form-data` and `text/plain`.

  - Added checking of the `Origin` header for `POST` requests.

  - The version number of Firefox is now checked when establishing a session.

### Known problems

- _macOS 10.15 (Catalina):_

  Due to the requirement from Apple that all programs must be
  notarized, geckodriver will not work on Catalina if you manually
  download it through another notarized program, such as Firefox.

  Whilst we are working on a repackaging fix for this problem, you can
  find more details on how to work around this issue in the [macOS
  notarization] section of the documentation.

- _Android:_

  Marionette will only be enabled in GeckoView based applications when the
  Firefox preference `devtools.debugger.remote-enabled` is set to `True` via
  [`moz:firefoxOptions`]. This will be fixed in one of the upcoming Firefox
  for Android releases.

  In some cases geckodriver could crash due to a stack overflow when handling
  certain device errors.

### Added

- To set environment variables for the launched Firefox for Android,
  it is now possible to add an `env` object on [`moz:firefoxOptions`]
  (note: this is not supported for Firefox Desktop)

- Support for print-to-PDF

  The newly standardised WebDriver [Print] endpoint provides a way to
  render pages to a paginated PDF representation. This endpoint is
  supported by geckodriver when using Firefox version ≥78.

- Support for same-site cookies

  Cookies can now be set with a `same-site` parameter, and the value
  of that parameter will be returned when cookies are
  retrieved. Requires Firefox version ≥79. Thanks to [Peter Major] for
  the patch.

### Fixed

- _Android:_

  * Firefox running on Android devices can now be controlled from a Windows host.

  * Setups with multiple connected Android devices are now supported.

  * Improved cleanup of configuration files. This prevents crashes if
    the application is started manually after launching it through
    geckodriver.

- Windows and Linux binaries are again statically linked.

0.26.0  (2019-10-12, `e9783a644016'`)
-------------------------------------

Note that with this release the minimum recommended Firefox version
has changed to Firefox ≥60.

### Known problems

- _macOS 10.15 (Catalina):_

  Due to the recent requirement from Apple that all programs must
  be notarized, geckodriver will not work on Catalina if you manually
  download it through another notarized program, such as Firefox.

  Whilst we are working on a repackaging fix for this problem, you
  can find more details on how to work around this issue in the
  [macOS notarization] section of the documentation.

- _Windows:_

  You must still have the [Microsoft Visual Studio redistributable
  runtime] installed on your system for the binary to run.  This
  is a known bug which we weren't able fix for this release.

- _Android:_

  Marionette will only be enabled in GeckoView based applications when the
  Firefox preference `devtools.debugger.remote-enabled` is set to `True` via
  [`moz:firefoxOptions`]. This will be fixed in one of the upcoming Firefox
  for Android releases.

  In some cases geckodriver could crash due to a stack overflow when handling
  certain device errors.

### Added

- Support for Firefox on Android

  Starting with this release geckodriver is able to connect to
  Firefox on Android systems, and to control packages based on
  [GeckoView].

  Support for Android works by the geckodriver process running on
  a host system and Firefox running within either an emulator or
  on a physical device connected to the host system.  This requires
  you to first [enable remote debugging on the Android device].

  The WebDriver client must set the [`platformName` capability] to
  "`android`" and the `androidPackage` capability within
  [`moz:firefoxOptions`] to the Android package name of the Firefox
  application.

  The full list of new capabilities specific to Android, instructions
  how to use them, and examples can be found in the [`moz:firefoxOptions`]
  documentation on MDN.

  When the session is created, the `platformName` capability will
  return "`android`" instead of reporting the platform of the host
  system.

### Changed

- Continued Marionette refactoring changes

  0.25.0 came with a series of internal changes for how geckodriver
  communicates with Firefox over the Marionette protocol.  This
  release contains the second half of the refactoring work.

### Fixed

- Connection attempts to Firefox made more reliable

  geckodriver now waits for the Marionette handshake before assuming
  the session has been established.  This should improve reliability
  in creating new WebDriver sessions.

- Corrected error codes used during session creation

  When a new session was being configured with invalid input data,
  the error codes returned was not always consistent.  Attempting
  to start a session with a malformed capabilities configuration
  will now return the [`invalid argument`] error consistently.


0.25.0 (2019-09-09, `bdb64cf16b68`)
-----------------------------------

__Note to Windows users!__
With this release you must have the [Microsoft Visual Studio redistributable runtime]
installed on your system for the binary to run.
This is a [known bug](https://github.com/mozilla/geckodriver/issues/1617)
with this particular release that we intend to release a fix for soon.

### Added

- Added support for HTTP `HEAD` requests to the HTTPD

  geckodriver now responds correctly to HTTP `HEAD` requests,
  which can be used for probing whether it supports a particular API.

  Thanks to [Bastien Orivel] for this patch.

- Added support for searching for Nightly’s default path on macOS

  If the location of the Firefox binary is not given, geckodriver
  will from now also look for the location of Firefox Nightly in
  the default locations.  The ordered list of search paths on macOS
  is as follows:

    1. `/Applications/Firefox.app/Contents/MacOS/firefox-bin`
    2. `$HOME/Applications/Firefox.app/Contents/MacOS/firefox-bin`
    3. `/Applications/Firefox Nightly.app/Contents/MacOS/firefox-bin`
    4. `$HOME/Applications/Firefox Nightly.app/Contents/MacOS/firefox-bin`

  Thanks to [Kriti Singh] for this patch.

- Support for application bundle paths on macOS

  It is now possible to pass an application bundle path, such as
  `/Applications/Firefox.app` as argument to the `binary` field in
  [`moz:firefoxOptions`].  This will be automatically resolved to
  the absolute path of the binary when Firefox is started.

  Thanks to [Nupur Baghel] for this patch.

- macOS and Windows builds are signed

  With this release of geckodriver, executables for macOS and Windows
  are signed using the same certificate key as Firefox.  This should
  help in cases where geckodriver previously got misidentified as
  a virus by antivirus software.

### Removed

- Dropped support for legacy Selenium web element references

  The legacy way of serialising web elements, using `{"ELEMENT": <UUID>}`,
  has been removed in this release.  This may break older Selenium
  clients and clients which are otherwise not compatible with the
  WebDriver standard.

  Thanks to [Shivam Singhal] for this patch.

- Removed `--webdriver-port` command-line option

  `--webdriver-port <PORT>` was an undocumented alias for `--port`,
  initially used for backwards compatibility with clients
  prior to Selenium 3.0.0.

### Changed

- Refactored Marionette serialisation

  Much of geckodriver’s internal plumbing for serialising WebDriver
  requests to Marionette messages has been refactored to decrease
  the amount of manual lifting.

  This work should have no visible side-effects for users.

  Thanks to [Nupur Baghel] for working on this throughout her
  Outreachy internship at Mozilla.

- Improved error messages for incorrect command-line usage

### Fixed

- Errors related to incorrect command-line usage no longer hidden

  By mistake, earlier versions of geckodriver failed to print incorrect
  flag use.  With this release problems are again written to stderr.

- Search system path for Firefox binary on BSDs

  geckodriver would previously only search the system path for the
  `firefox` binary on Linux.  Now it supports different BSD flavours
  as well.


0.24.0 (2019-01-28, `917474f3473e`)
-----------------------------------

### Added

- Introduces `strictFileInteractability` capability

  The new capability indicates if strict interactability checks
  should be applied to `<input type=file>` elements.  As strict
  interactability checks are off by default, there is a change
  in behaviour when using [Element Send Keys] with hidden file
  upload controls.

- Added new endpoint `GET /session/{session id}/moz/screenshot/full`
  for taking full document screenshots, thanks to Greg Fraley.

- Added new `--marionette-host <hostname>` flag for binding to a
  particular interface/IP layer on the system.

- Added new endpoint `POST /session/{session_id}/window/new`
  for the [New Window] command to create a new top-level browsing
  context, which can be either a window or a tab. The first version
  of Firefox supporting this command is Firefox 66.0.

- When using the preference `devtools.console.stdout.content` set to
  `true` logging of console API calls like `info()`, `warn()`, and
  `error()` can be routed to stdout.

- geckodriver now sets the `app.update.disabledForTesting` preference
  to prevent Firefox >= 65 from automatically updating whilst under
  automation.

### Removed

- ARMv7 HF builds have been discontinued

  We [announced](https://lists.mozilla.org/pipermail/tools-marionette/2018-September/000035.html)
  back in September 2018 that we would stop building for ARM,
  but builds can be self-serviced by building from source.

  To cross-compile from another host system, you can use this command:

  	% cargo build --target armv7-unknown-linux-gnueabihf

### Changed

- Allow file uploads to hidden `<input type=file>` elements

  Through a series of changes to the WebDriver specification,
  geckodriver is now aligned with chromedriver’s behaviour that
  allows interaction with hidden `<input type=file>` elements.

  This allows WebDriver to be used with various popular web
  frameworks that—through indirection—hides the file upload control
  and invokes it through other means.

- Allow use of an indefinite script timeout for the [Set Timeouts]
  command, thanks to reimu.

### Fixed

- Corrected `Content-Type` of response header to `utf-8` to fix
  an HTTP/1.1 compatibility bug.

- Relaxed the deserialization of timeouts parameters to allow unknown
  fields for the [Set Timeouts] command.

- Fixed a regression in the [Take Element Screenshot] to not screenshot
  the viewport, but the requested element.


0.23.0 (2018-10-03)
-------------------

This release contains a number of fixes for regressions introduced
in 0.22.0, where we shipped a significant refactoring to the way
geckodriver internally dealt with JSON serialisation.

### Removed

- The POST `/session/{session id}/element/{element id}/tap` endpoint
  was removed, thanks to Kerem Kat.

### Changed

- [webdriver crate] upgraded to 0.38.0.

### Fixed

- `desiredCapabilities` and `requiredCapabilities` are again
  recognised on session creation

  A regression in 0.22.0 caused geckodriver to recognise `desired`
  and `required` instead of the correct `desiredCapabilities`
  and `requiredCapabilities`.  This will have caused significant
  problems for users who relied on this legacy Selenium-style
  session creation pattern.

  Do however note that support for Selenium-styled new session
  requests is temporary and that this will be removed sometime
  before the 1.0 release.

- `duration` field made optional on pause actions

  A regression in 0.22.0 caused the pause action primitive to
  require a `duration` field.  This has now been fixed so that
  pauses in action chains can be achieved with the default duration.

- Log level formatted to expected Marionette input

  A regression in 0.22.0 caused the log level to be improperly
  formatted when using Firefox pre-releases.  This is now fixed so
  that the requested log level is correctly interpreted by Marionette.

- `temporary` field on addon installation made optional

  A regression in 0.22.0 caused the `temporary` field for POST
  `/session/{session id}/moz/addon/install` to be mandatory.  This has
  now been fixed so that an addon is installed permanently by default.

- SHA1s in version information uses limited number of characters

  The SHA1 used in `--version` when building geckodriver from a
  git repository is now limited to 12 characters, as it is when
  building from an hg checkout.  This ensures reproducible builds.


0.22.0 (2018-09-15)
-------------------

This release marks an important milestone on the path towards
a stable release of geckodriver.  Large portions of geckodriver
and the [webdriver] library it is based on has been refactored to
accommodate using [serde] for JSON serialization.

We have also made great strides to improving [WebDriver conformance],
to the extent that geckodriver is now _almost_ entirely conforming
to the standard.

### Added

- Support for WebDriver web element-, web frame-, and web window
  identifiers from Firefox.

- Added support for the non-configurable `setWindowRect` capability
  from WebDriver.

  This capability informs whether the attached browser supports
  manipulating the window dimensions and position.

- A new extension capability `moz:geckodriverVersion` is returned
  upon session creation.

### Changed

- All JSON serialization and deserialisation has moved from
  rustc_serialize to [serde].

- The HTTP status codes used for [script timeout] and [timeout]
  errors has changed from Request Timeout (408) to Internal Server
  Error (500) in order to not break HTTP/1.1 `Keep-Alive` support,
  as HTTP clients interpret the old status code to mean they should
  duplicate the request.

- The HTTP/1.1 `Keep-Alive` timeout for persistent connections  has
  been increased to 90 seconds.

- An [invalid session ID] error is now returned when there is no
  active session.

- An [invalid argument] error is now returned when [Add Cookie]
  is given invalid parameters.

- The handshake when geckodriver connects to Marionette has been
  hardened by killing the Firefox process if it fails.

- The handshake read timeout has been reduced to 10 seconds instead
  of waiting forever.

- The HTTP server geckodriver uses, [hyper], has been upgraded to
  version 0.12, thanks to [Bastien Orivel].

- geckodriver version number is no longer logged on startup, as
  the log level is not configured until a session is created.

  The version number is available through `--version`, and now
  also through a new `moz:geckodriverVersion` field in the matched
  capabilities.

- [webdriver crate] upgraded to 0.37.0.

### Fixed

- Parsing [timeout object] values has been made WebDriver conforming,
  by allowing floats as input.

- Implicit downloads of OpenH264 and Widevine plugins has been disabled.

- The commit hash and date displayed when invoking `--version`
  is now well-formatted when built from an hg repository, thanks to
  [Jeremy Lempereur].

- Many documentation improvements, now published on
  https://firefox-source-docs.mozilla.org/testing/geckodriver/.


0.21.0 (2018-06-15)
-------------------

Note that with this release of geckodriver the minimum recommended
Firefox and Selenium versions have changed:

  - Firefox 57 (and greater)
  - Selenium 3.11 (and greater)

### Added

- Support for the chrome element identifier from Firefox.

- The `unhandledPromptBehavior` capability now accepts `accept and
  notify`, `dismiss and notify`, and `ignore` options.

  Note that the unhandled prompt handler is not fully supported in
  Firefox at the time of writing.

### Changed

- Firefox will now be started with the `-foreground` and `-no-remote`
  flags if they have not already been specified by the user in
  [`moz:firefoxOptions`].

  `-foreground` will ensure the application window gets focus when
  Firefox is started, and `-no-remote` will prevent remote commands
  to this instance of Firefox and also ensure we always start a new
  instance.

- WebDriver commands that do not have a return value now correctly
  return `{value: null}` instead of an empty dictionary.

- The HTTP server now accepts `Keep-Alive` connections.

- Firefox remote protocol command mappings updated.

  All Marionette commands changed to make use of the `WebDriver:`
  prefixes introduced with Firefox 56.

- Overhaul of Firefox preferences.

  Already deprecated preferences in Firefox versions earlier than
  57 got removed.

- [webdriver crate] upgraded to 0.36.0.

### Fixed

- Force use of IPv4 network stack.

  On certain system configurations, where `localhost` resolves to
  an IPv6 address, geckodriver would attempt to connect to Firefox
  on the wrong IP stack, causing the connection attempt to time out
  after 60 seconds.  We now ensure that geckodriver uses IPv4
  consistently to both connect to Firefox and for allocating a free
  port.

- geckodriver failed to locate the correct Firefox binary if it was
  found under a _firefox_ or _firefox-bin_ directory, depending on
  the system, because it thought the parent directory was the
  executable.

- On Unix systems (macOS, Linux), geckodriver falsely reported
  non-executable files as valid binaries.

- When stdout and stderr is redirected by geckodriver, a bug prevented
  the redirections from taking effect.


0.20.1 (2018-04-06)
-------------------

### Fixed

- Avoid attempting to kill Firefox process that has stopped.

  With the change to allow Firefox enough time to shut down in
  0.20.0, geckodriver started unconditionally killing the process
  to reap its exit status.  This caused geckodriver to inaccurately
  report a successful Firefox shutdown as a failure.

  The regression should not have caused any functional problems, but
  the termination cause and the exit status are now reported correctly.


0.20.0 (2018-03-08)
-------------------

### Added

- New `--jsdebugger` flag to open the [Browser Toolbox] when Firefox
  launches.  This is useful for debugging Marionette internals.

- Introduced the temporary, boolean capability
  `moz:useNonSpecCompliantPointerOrigin` to disable the WebDriver
  conforming behavior of calculating the Pointer Origin.

### Changed

- HTTP status code for the [`StaleElementReference`] error changed
  from 400 (Bad Request) to 404 (Not Found).

- Backtraces from geckodriver no longer substitute for missing
  Marionette stacktraces.

- [webdriver crate] upgraded to 0.35.0.

### Fixed

- The Firefox process is now given ample time to shut down, allowing
  enough time for the Firefox shutdown hang monitor to kick in.

  Firefox has an integrated background monitor that observes
  long-running threads during shutdown.  These threads will be
  killed after 63 seconds in the event of a hang.  To allow Firefox
  to shut down these threads on its own, geckodriver has to wait
  that time and some additional seconds.

- Grapheme clusters are now accepted as input for keyboard input
  to actions.

  Input to the `value` field of the `keyDown` and `keyUp` action
  primitives used to only accept single characters, which means
  geckodriver would error when a valid grapheme cluster was sent in,
  for example with the tamil nadu character U+0BA8 U+0BBF.

  Thanks to Greg Fraley for fixing this bug.

- Improved error messages for malformed capability values.


0.19.1 (2017-10-30)
-------------------

### Changed

- Search suggestions in the location bar turned off as not to
  trigger network connections

- Block addons incompatible with E10s

### Fixed

- Marionette stacktraces are now correctly propagated

- Some error messages have been clarified

### Removed

- Removed obsolete `socksUsername` and `socksPassword` proxy
  configuration keys because neither were picked up or recognised


0.19.0 (2017-09-16)
-------------------

Note that with geckodriver 0.19.0 the following versions are recommended:
- Firefox 55.0 (and greater)
- Selenium 3.5 (and greater)

### Added

- Added endpoint:
  - POST `/session/{session id}/window/minimize` for the [Minimize Window]
    command

- Added preference `extensions.shield-recipe-client.api_url` to disable
  shield studies which could unexpectedly change the behavior of Firefox

- Introduced the temporary, boolean capability `moz:webdriverClick` to
  enable the WebDriver conforming behavior of the [Element Click] command

- Added crashreporter environment variables to better control the browser
  in case of crashes

- Added preference `dom.file.createInChild` set to true to allow file
  object creation in content processes

### Changed

- Log all used application arguments and not only `-marionette`

- Early abort connection attempts to Marionette if the Firefox process
  closed unexpectetly

- Removed deprecated `socksProxyVersion` in favor of `socksVersion`

- Removed `ftpProxyPort`, `httpProxyPort`, `sslProxyPort`, and
  `socksProxyPort` because _ports_ have to be set for `ftpProxy`,
  `httpProxy`, `sslProxy`, and `socksProxy` using ":<PORT>"

- The `proxyType` `noproxy` has been replaced with `direct` in accordance
  with recent WebDriver specification changes

- The [`WindowRectParameters`] have been updated to return signed 32-bit
  integers in accordance with the CSS and WebDriver specifications, and
  to be more liberal with the input types

- Mapped the [`FullscreenWindow`] to the correct Marionette command

- To make sure no browser process is left behind when the [`NewSession`]
  fails, the process is closed immediately now

- `/moz/addon/install` command accepts an `addon` parameter, in lieu of
  `path`, containing an addon as a Base64 string (fixed by [Jason Juang])

- [webdriver crate] upgraded to version 0.31.0

- [mozrunner crate] upgraded to version 0.5.0

### Removed

- Removed the following obsolete preferences for Firefox:
  - `browser.safebrowsing.enabled`
  - `browser.safebrowsing.forbiddenURIs.enabled`
  - `marionette.defaultPrefs.port`
  - `marionette.logging`


0.18.0 (2017-07-10)
-------------------

### Changed

- [`RectResponse`] permits returning floats for `width` and `height`
  fields

- New type [`CookieResponse`] for the [`GetNamedCookie`] command returns
  a single cookie, as opposed to an array of a single cookie

- To pick up a prepared profile from the filesystem, it is now possible
  to pass `["-profile", "/path/to/profile"]` in the `args` array on
  [`moz:firefoxOptions`]

- geckodriver now recommends Firefox 53 and greater

- Version information (`--version`) contains the hash from from the
  commit used to build geckodriver

- geckodriver version logged on startup

- [webdriver crate] upgraded to version 0.27.0

- [mozrunner crate] upgraded to version 0.4.1

### Fixed

- The [`SetTimeouts`] command maps to the Marionette `setTimeouts`
  command, which makes geckodriver compatible with Firefox 56 and greater

- Linux x86 (i686-unknown-linux-musl) builds are fixed


0.17.0 (2017-06-09)
-------------------

### Added

- Added endpoints:
  - POST `/session/{session id}/window/fullscreen` to invoke the window
    manager-specific `full screen` operation
  - POST `/session/{session id}/moz/addon/install` to install an extension
    (Gecko only)
  - POST `/session/{session id}/moz/addon/uninstall` to uninstall an
    extension (Gecko only)

### Changed

- Increasing the length of the `network.http.phishy-userpass-length`
  preference will cause Firefox to not prompt when navigating to a
  website with a username or password in the URL

- Library dependencies upgraded to mozrunner 0.4 and mozprofile 0.3
  to allow overriding of preferences via capabilities if those have been
  already set in the profile

- Library dependencies upgraded to mozversion 0.1.2 to only use the
  normalized path of the Firefox binary for version checks but not to
  actually start the browser, which broke several components in Firefox
  on Windows

### Fixed

- The [SetWindowRect] command now returns the [WindowRectResponse]
  when it is done

- Use ASCII versions of array symbols to properly display them in the
  Windows command prompt

- Use [`SessionNotCreated`] error instead of [`UnknownError`] if there
  is no current session


0.16.1 (2017-04-26)
-------------------

### Fixed

- Read Firefox version number from stdout when failing
  to look for the application .ini file (fixes [Selenium
  #3884](https://github.com/SeleniumHQ/selenium/issues/3884))

- Session is now ended when closing the last Firefox window (fixes
  [#613](https://github.com/mozilla/geckodriver/issues/613))


0.16.0 (2017-04-21)
-------------------

Note that geckodriver v0.16.0 is only compatible with Selenium 3.4
and greater.

### Added

- Support for WebDriver-conforming [New Session] negotiation, with
  `desiredCapabilities`/`requiredCapabilities` negotiation as fallback

- Added two new endpoints:
  - GET `/session/{session id}/window/rect` for [Get Window Rect]
  - POST `/session/{session id}/window/rect` for [Set Window Rect]

- Align errors with the [WebDriver errors]:
  - Introduces new errors [`ElementClickIntercepted`],
  [`ElementNotInteractable`], [`InvalidCoordinates`], [`NoSuchCookie`],
  [`UnableToCaptureScreen`], and [`UnknownCommand`]
  - Removes `ElementNotVisible` and `InvalidElementCoordinates` errors

### Removed

- Removed following list of unused endpoints:
  - GET `/session/{session id}/alert_text`
  - POST `/session/{session id}/alert_text`
  - POST `/session/{session id}/accept_alert`
  - POST `/session/{session id}/dismiss_alert`
  - GET `/session/{session id}/window_handle`
  - DELETE `/session/{session id}/window_handle`
  - POST `/session/{session id}/execute_async`
  - POST `/session/{session id}/execute`

### Changed

- [`SendKeysParameters`], which is used for the [Element Send Keys] and
  [Send Alert Text] commands, has been updated to take a string `text`
  field

- [`CookieResponse`] and [`CloseWindowResponse`] fixed to be properly
  wrapped in a `value` field, like other responses

- Allow negative numbers for `x` and `y` fields in `pointerMove` action

- Disable Flash and the plugin container in Firefox by
  default, which should help mitigate the “Plugin Container
  for Firefox has stopped working” problems [many users were
  reporting](https://github.com/mozilla/geckodriver/issues/225) when
  deleting a session

- Preferences passed in a profile now take precedence over
  set of default preferences defined by geckodriver (fixed by
  [Marc Fisher](https://github.com/DrMarcII))
  - The exceptions are the `marionette.port` and `marionette.log.level`
    preferences and their fallbacks, which are set unconditionally and
    cannot be overridden

- Remove default preference that disables unsafe CPOW checks

- WebDriver library updated to 0.25.2

### Fixed

- Fix for the “corrupt deflate stream” exception that
  sometimes occurred when trying to write an empty profile by
  [@kirhgoph](https://github.com/kirhgoph)

- Recognise `sslProxy` and `sslProxyPort` entries in the proxy
  configuration object (fixed by [Jason Juang])

- Fix “`httpProxyPort` was not an integer” error (fixed by [Jason
  Juang])

- Fix broken unmarshaling of _Get Timeouts_ response format from Firefox
  52 and earlier (fixed by [Jason Juang])

- Allow preferences in [`moz:firefoxOptions`] to be both positive- and
  negative integers (fixed by [Jason Juang])

- Allow IPv6 hostnames in the proxy configuration object

- i686-unknown-linux-musl (Linux 32-bit) build fixed

- Log messages from other Rust modules are now ignored

- Improved log messages to the HTTPD


0.15.0 (2017-03-08)
-------------------

### Added

- Added routing and parsing for the [Get Timeouts] command

### Changed

- All HTTP responses are now wrapped in `{value: …}` objects per the
  WebDriver specification; this may likely require you to update your
  client library

- Pointer move action’s `element` key changed to `origin`, which
  lets pointer actions originate within the context of the viewport,
  the pointer’s current position, or from an element

- Now uses about:blank as the new tab document; this was previously
  disabled due to [bug 1333736](https://bugzil.la/1333736) in Marionette

- WebDriver library updated to 0.23.0

### Fixed

- Aligned the data structure accepted by the [Set Timeouts] command with
  the WebDriver specification


0.14.0 (2017-01-31)
-------------------

### Changed

- Firefox process is now terminated and session ended when the last
  window is closed

- WebDriver library updated to version 0.20.0

### Fixed

- Stacktraces are now included when the error originates from within
  the Rust stack

- HTTPD now returns correct response headers for `Content-Type` and
  `Cache-Control` thanks to [Mike Pennisi]


0.13.0 (2017-01-06)
-------------------

### Changed

- When navigating to a document with an insecure- or otherwise invalid
  TLS certificate, an [insecure certificate] error will be returned

- On macOS, deducing Firefox’ location on the system will look for
  _firefox-bin_ on the system path (`PATH` environmental variable) before
  looking in the applications folder

- Window position coordinates are allowed to be negative numbers, to
  cater for maximised window positioning on Windows

- WebDriver library updated to version 0.18.0

### Fixed

- Check for single-character key codes in action sequences now counts
  characters instead of bytes


0.12.0 (2017-01-03)
-------------------

### Added

- Added [Take Element Screenshot] command

- Added new [Status] command

- Added routing for the [Get Timeouts] command, but it is not yet
  implemented in Marionette, and will return an _unsupported operation_
  error until it is

- Implemented routing for [new actions API](Actions), but it too is not
  yet fully implemented in Marionette

### Changed

- [Synced Firefox
  preferences](https://github.com/mozilla/geckodriver/commit/2bfdc3ec8151c427a6a75a6ba3ad203459540495)
  with those used in Mozilla automation

- Default log level for debug builds of Firefox, which used to be `DEBUG`,
  changed to `INFO`-level

- WebDriver library dependency upgraded to 0.17.1

- Using _session not created_ error when failing to start session

- geckodriver will exit with exit code 69 to indicate that the port
  is unavailable

### Fixed

- Improved logging when starting Firefox

- Reverted to synchronous logging, which should address cases of
  inconsistent output when failing to bind to port

- Clarified in README that geckodriver is not supported on Windows XP

- Added documentation of supported capabilities to [README]

- Included capabilities example in the [README]


0.11.1 (2016-10-10)
-------------------

### Fixed

- Version number in binary now reflects the release version


0.11.0 (2016-10-10)
-------------------

### Added

- Introduced continuous integration builds for Linux- and Windows 32-bit
  binaries

- Added commands for setting- and getting the window position

- Added new extension commands for finding an element’s anonymous
  children and querying its attributes; accessible through the
  `/session/{sessionId}/moz/xbl/{elementId}/anonymous_children`
  to return all anonymous children and
  `/session/{sessionId}/moz/xbl/{elementId}/anonymous_by_attribute` to
  return an anonymous element by a name and attribute query

- Introduced a [`moz:firefoxOptions`] capability to customise a Firefox
  session:

  - The `binary`, `args`, and `profile` entries on this dictionary
    is equivalent to the old `firefox_binary`, `firefox_args`, and
    `firefox_profile` capabilities, which have now all been removed

  - The `log` capability takes a dictionary such as `{log: "trace"}`
    to enable trace level verbosity in Gecko

  - The `prefs` capability lets you define Firefox preferences through
    capabilities

- Re-introduced the `--webdriver-port` argument as a hidden alias to
  `--port`

### Changed

- `firefox_binary`, `firefox_args`, and `firefox_profile` capabilities
  removed in favour of the [`moz:firefoxOptions`] dictionary detailed above
  and in the [README]

- Removed `--no-e10s` flag, and geckodriver will from now rely on the
  Firefox default multiprocessing settings (override using preferences)

- Disable pop-up blocker in the default profile by @juangj

- Changed Rust compiler version to 1.12 (beta)
  temporarily because of [trouble linking Musl
  binaries](https://github.com/rust-lang/rust/issues/34978)

- Replaced _env_logger_ logging facility with the _slog_ package,
  causing the `RUST_LOG` environment variable to no longer have any affect

- Updated the WebDriver Rust library to version 0.15

### Fixed

- Corrected link to repository in Cargo metadata

- Verbosity shorthand flag `-v[v]` now works again, following the
  replacement of the argument parsing library in the previous release

- When the HTTPD fails to start, errors are propagated to the user

- Disabled the additional welcome URL
  (`startup.homepage_welcome_url.additional`) so that officially branded
  Firefox builds do not start with two open tabs in fresh profiles

- Disabled homepage override URL redirection on milestone upgrades,
  which means a tab with an upgrade notice is not displayed when launching
  a new Firefox version


0.10.0 (2016-08-02)
-------------------

### Changed

- Use multi-process Firefox (e10s) by default, added flag `--no-e10s`
  to disable it and removed `--e10s` flag

- Disable autofilling of forms by default by [Sven Jost]

- Replace _argparse_ with _clap_ for arguments parsing

### Fixed

- Attempt to deploy a single file from Travis when making a release

- Grammar fix in [README]


0.9.0 (2016-06-30)
------------------

### Added

- Add ability to use `firefox_binary` capability to define location of
  Firefox to use

- Automatically detect the default Firefox path if one is not given

- Cross-compile to Windows and ARMv7 (HF) in CI

- Add Musl C library-backed static binaries in CI

- Add `-v`, `-vv`, and `--log LEVEL` flags to increase Gecko verbosity

- Add Get Element Property endpoint

- Add new `--version` flag showing copying information and a link to
  the repository

### Changed

- Now connects to a Marionette on a random port by default

- Update webdriver-rust library dependency

- Migrated to use Travis to deploy new releases

- Reduced amount of logging

- Introduced a changelog (this)


0.8.0 (2016-06-07)
------------------

### Added

- Allow specifying array of arguments to the Firefox binary through the
  `firefox_args` capability

- Pass parameters with [New Session] command

### Changed

- Change product name to _geckodriver_

- Make README more exhaustive

- Quit Firefox when deleting a session

- Update webdriver-rust library

- Update dependencies

### Fixed

- Fix tests

- FIx typo in error message for parsing errors


0.7.1 (2016-04-27)
------------------

### Added

- Add command line flag for using e10s enabled Firefox by [Kalpesh
  Krishna]

- Allow providing custom profiles

### Changed

- Allow binding to an IPv6 address by [Jason Juang]

- By default, connect to host-agnostic localhost by [Jason Juang]

- Make `GeckoContextParameters` public

- Update dependencies

### Fixed

- Squash rustc 1.6 warnings by using `std::thread::sleep(dur: Duration)`


0.6.2 (2016-01-20)
------------------

### Added

- Add LICENSE file from [Joshua Burning]

- Schedule builds in CI on pushes and pull requests

### Changed

- Enable CPOWs in Marionette


0.6.0 (2016-01-12)
------------------

### Added

- Add Get Page Source endpoint

### Changed

- Handle arrays being sent from Marionette

- Correct build steps in [README]

- Update what properties are read from errors sent by Marionette

- Update dependencies


0.5.0 (2015-12-10)
------------------

### Changed

- Update argparse dependency to use Cargo

- Update to the latest version of the Marionette wire protocol

- Update to latest webdriver-rust library

- Update dependencies


0.4.2 (2015-10-02)
------------------

### Changed

- Skip compiling optional items in hyper


0.4.1 (2015-10-02)
------------------

### Changed

- Update webdriver-rust library

- Update dependencies


0.4.0 (2015-09-28)
------------------

### Added

- Add command extensions for switching between content- and chrome
  contexts

- Add more documentation from [Vlad Filippov]

### Changed

- Update Cargo.lock with new dependencies for building

- Update for protocol updates that flatten commands

- Update to new protocol error handling

- Update for Marionette protocol version 3 changes

- Strip any leading and trailing `{}` from the `sessionId` Marionette
  returns

- Update dependencies

### Fixed

- Fix `GetCSSValue` message to send correct key `propertyName`

- Fix example in documentation from @vladikoff


0.3.0 (2015-08-17)
------------------

### Added

- Add support for finding elements in subtrees


0.2.0 (2015-05-20)
------------------

### Added

- Extra debug messages

- Add ability to set WebDriver port

- Add support for getting the active element

- Add support for `GetCookies` and `DeleteCookie`/`DeleteCookies`

- Add preferences that switch off certain features not required for
  WebDriver tests

### Changed

- Make failing to communicate with Firefox a fatal error that closes
  the session

- Shut down session only when losing connection

- Better handling of missing command line flags

- Poll for connection every 100ms rather than every 100s

- Switch to string-based error codes

- Switch webdriver-rust library dependency to be pulled from git

- Update dependencies

### Fixed

- Handle null id for switching to frame more correctly


0.1.0 (2015-04-09)
------------------

### Added

- Add proxy for converting WebDriver HTTP protocol to Marionette protocol

- Add endpoints for modal dialogue support

- Allow connecting to a running Firefox instance

- Add explicit Cargo.lock file

- Start Firefox when we get a [NewSession] command

- Add flag parsing and address parsing

- Add basic error handling

### Changed

- Update for Rust beta

- Switch to new IO libraries

- Pin webdriver-rust commit so we can upgrade rustc versions independently

- Set preferences when starting Firefox

- Improve some error messages

- Re-enable environment variable based logging

### Fixed

- Fix Get Element Rect command to return floats instead of integers

- Fix passing of web elements to Switch To Frame command

- Fix serialisation of script commands

- Fix assorted bugs found by the Selenium test suite

- Fix conversion of Find Element/Find Elements responses from Marionette
  to WebDriver

- Fixed build by updating Cargo.lock with new dependencies for building

- Squash compile warnings



[README]: https://github.com/mozilla/geckodriver/blob/master/README.md
[Browser Toolbox]: https://developer.mozilla.org/en-US/docs/Tools/Browser_Toolbox
[WebDriver conformance]: https://wpt.fyi/results/webdriver/tests?label=experimental
[`moz:firefoxOptions`]: https://developer.mozilla.org/en-US/docs/Web/WebDriver/Capabilities/firefoxOptions
[`moz:debuggerAddress`]: https://firefox-source-docs.mozilla.org/testing/geckodriver/Capabilities.html#moz-debuggeraddress
[Microsoft Visual Studio redistributable runtime]: https://support.microsoft.com/en-us/help/2977003/the-latest-supported-visual-c-downloads
[GeckoView]: https://wiki.mozilla.org/Mobile/GeckoView
[Fission]: https://wiki.mozilla.org/Project_Fission
[Capabilities]: https://firefox-source-docs.mozilla.org/testing/geckodriver/Capabilities.html
[Flags]: https://firefox-source-docs.mozilla.org/testing/geckodriver/Flags.html
[enable remote debugging on the Android device]: https://developers.google.com/web/tools/chrome-devtools/remote-debugging
[macOS notarization]: https://firefox-source-docs.mozilla.org/testing/geckodriver/Notarization.html

[`CloseWindowResponse`]: https://docs.rs/webdriver/newest/webdriver/response/struct.CloseWindowResponse.html
[`CookieResponse`]: https://docs.rs/webdriver/newest/webdriver/response/struct.CookieResponse.html
[`DeleteSession`]: https://docs.rs/webdriver/newest/webdriver/command/enum.WebDriverCommand.html#variant.DeleteSession
[`ElementClickIntercepted`]: https://docs.rs/webdriver/newest/webdriver/error/enum.ErrorStatus.html#variant.ElementClickIntercepted
[`ElementNotInteractable`]: https://docs.rs/webdriver/newest/webdriver/error/enum.ErrorStatus.html#variant.ElementNotInteractable
[`FullscreenWindow`]: https://docs.rs/webdriver/newest/webdriver/command/enum.WebDriverCommand.html#variant.FullscreenWindow
[`GetNamedCookie`]: https://docs.rs/webdriver/newest/webdriver/command/enum.WebDriverCommand.html#variant.GetNamedCookie
[`GetWindowRect`]: https://docs.rs/webdriver/newest/webdriver/command/enum.WebDriverCommand.html#variant.GetWindowRect
[`InvalidCoordinates`]: https://docs.rs/webdriver/newest/webdriver/error/enum.ErrorStatus.html#variant.InvalidCoordinates
[`MaximizeWindow`]: https://docs.rs/webdriver/newest/webdriver/command/enum.WebDriverCommand.html#variant.MaximizeWindow
[`MinimizeWindow`]: https://docs.rs/webdriver/newest/webdriver/command/enum.WebDriverCommand.html#variant.MinimizeWindow
[`NewSession`]: https://docs.rs/webdriver/newest/webdriver/command/enum.WebDriverCommand.html#variant.NewSession
[`NoSuchCookie`]: https://docs.rs/webdriver/newest/webdriver/error/enum.ErrorStatus.html#variant.NoSuchCookie
[`RectResponse`]: https://docs.rs/webdriver/0.27.0/webdriver/response/struct.RectResponse.html
[`SendKeysParameters`]: https://docs.rs/webdriver/newest/webdriver/command/struct.SendKeysParameters.html
[`SessionNotCreated`]: https://docs.rs/webdriver/newest/webdriver/error/enum.ErrorStatus.html#variant.SessionNotCreated
[`SetTimeouts`]: https://docs.rs/webdriver/newest/webdriver/command/enum.WebDriverCommand.html#variant.SetTimeouts
[`SetWindowRect`]: https://docs.rs/webdriver/newest/webdriver/command/enum.WebDriverCommand.html#variant.SetWindowRect
[`StaleElementReference`]: https://docs.rs/webdriver/newest/webdriver/error/enum.ErrorStatus.html#variant.StaleElementReference
[`UnableToCaptureScreen`]: https://docs.rs/webdriver/newest/webdriver/error/enum.ErrorStatus.html#variant.UnableToCaptureScreen
[`UnknownCommand`]: https://docs.rs/webdriver/newest/webdriver/error/enum.ErrorStatus.html#variant.UnknownCommand
[`UnknownError`]: https://docs.rs/webdriver/newest/webdriver/error/enum.ErrorStatus.html#variant.UnknownError
[`WindowRectParameters`]: https://docs.rs/webdriver/newest/webdriver/command/struct.WindowRectParameters.html

[Add Cookie]: https://developer.mozilla.org/en-US/docs/Web/WebDriver/Commands/AddCookie
[invalid argument]: https://developer.mozilla.org/en-US/docs/Web/WebDriver/Errors/InvalidArgument
[invalid session id]: https://developer.mozilla.org/en-US/docs/Web/WebDriver/Errors/InvalidSessionID
[script timeout]: https://developer.mozilla.org/en-US/docs/Web/WebDriver/Errors/ScriptTimeout
[timeout]: https://developer.mozilla.org/en-US/docs/Web/WebDriver/Errors/Timeout
[timeout object]: https://developer.mozilla.org/en-US/docs/Web/WebDriver/Timeouts
[`platformName` capability]: https://developer.mozilla.org/en-US/docs/Web/WebDriver/Capabilities#platformName

[hyper]: https://hyper.rs/
[mozrunner crate]: https://crates.io/crates/mozrunner
[serde]: https://serde.rs/
[webdriver crate]: https://crates.io/crates/webdriver

[Actions]: https://w3c.github.io/webdriver/webdriver-spec.html#actions
[Delete Session]: https://w3c.github.io/webdriver/webdriver-spec.html#delete-session
[Element Click]: https://w3c.github.io/webdriver/webdriver-spec.html#element-click
[Get Timeouts]: https://w3c.github.io/webdriver/webdriver-spec.html#get-timeouts
[Get Window Rect]: https://w3c.github.io/webdriver/webdriver-spec.html#get-window-rect
[insecure certificate]: https://w3c.github.io/webdriver/webdriver-spec.html#dfn-insecure-certificate
[Minimize Window]: https://w3c.github.io/webdriver/webdriver-spec.html#minimize-window
[New Session]: https://w3c.github.io/webdriver/webdriver-spec.html#new-session
[New Window]: https://developer.mozilla.org/en-US/docs/Web/WebDriver/Commands/New_Window
[Print]: https://w3c.github.io/webdriver/webdriver-spec.html#print
[Send Alert Text]: https://w3c.github.io/webdriver/webdriver-spec.html#send-alert-text
[Set Timeouts]: https://w3c.github.io/webdriver/webdriver-spec.html#set-timeouts
[Set Window Rect]: https://w3c.github.io/webdriver/webdriver-spec.html#set-window-rect
[Status]: https://w3c.github.io/webdriver/webdriver-spec.html#status
[Take Element Screenshot]: https://w3c.github.io/webdriver/webdriver-spec.html#take-element-screenshot
[WebDriver errors]: https://w3c.github.io/webdriver/webdriver-spec.html#handling-errors

[Bastien Orivel]: https://github.com/Eijebong
[Jason Juang]: https://github.com/juangj
[Jeremy Lempereur]: https://github.com/o0Ignition0o
[Joshua Bruning]: https://github.com/joshbruning
[Kalpesh Krishna]: https://github.com/martiansideofthemoon
[Kriti Singh]: https://github.com/kritisingh1
[Mike Pennisi]: https://github.com/jugglinmike
[Nupur Baghel]: https://github.com/nupurbaghel
[Peter Major]: https://github.com/aldaris
[Shivam Singhal]: https://github.com/championshuttler
[Sven Jost]: https://github/mythsunwind
[Vlad Filippov]: https://github.com/vladikoff
