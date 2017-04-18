# Change log

All notable changes to this program is documented in this file.

## Unreleased

### Added
- Support for WebDriver-conforming [New Session](https://w3c.github.io/webdriver/webdriver-spec.html#dfn-new-session) negotiation, with `desiredCapabilities`/`requiredCapabilities` negotiation as fallback
- Added two new endpoints:
  - GET `/session/{session id}/window/rect` for [Get Window Rect](https://w3c.github.io/webdriver/webdriver-spec.html#get-window-rect)
  - POST `/session/{session id}/window/rect` for [Set Window Rect](https://w3c.github.io/webdriver/webdriver-spec.html#set-window-rect)
- Align errors with the [WebDriver errors](https://w3c.github.io/webdriver/webdriver-spec.html#handling-errors):
  - Introduces new errors [`ElementClickIntercepted`](https://docs.rs/webdriver/0.25.0/webdriver/error/enum.ErrorStatus.html#variant.ElementClickIntercepted), [`ElementNotInteractable`](https://docs.rs/webdriver/0.25.0/webdriver/error/enum.ErrorStatus.html#variant.ElementNotInteractable), [`InvalidCoordinates`](https://docs.rs/webdriver/0.25.0/webdriver/error/enum.ErrorStatus.html#variant.InvalidCoordinates), [`NoSuchCookie`](https://docs.rs/webdriver/0.25.0/webdriver/error/enum.ErrorStatus.html#variant.NoSuchCookie), [`UnableToCaptureScreen`](https://docs.rs/webdriver/0.25.0/webdriver/error/enum.ErrorStatus.html#variant.UnableToCaptureScreen), and [`UnknownCommand`](https://docs.rs/webdriver/0.25.0/webdriver/error/enum.ErrorStatus.html#variant.UnknownCommand)
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
- [`SendKeysParameters`](https://docs.rs/webdriver/0.25.0/webdriver/command/struct.SendKeysParameters.html), which is used for the [Element Send Keys](https://w3c.github.io/webdriver/webdriver-spec.html#element-send-keys) and [Send Alert Text](https://w3c.github.io/webdriver/webdriver-spec.html#send-alert-text) commands, has been updated to take a string `text` field
- [`CookieResponse`](https://docs.rs/webdriver/0.25.0/webdriver/response/struct.CookieResponse.html) and [`CloseWindowResponse`](https://docs.rs/webdriver/0.25.0/webdriver/response/struct.CloseWindowResponse.html) fixed to be properly wrapped in a `value` field, like other responses
- Allow negative numbers for `x` and `y` fields in `pointerMove` action
- Disable Flash and the plugin container in Firefox by default, which should help mitigate the “Plugin Container for Firefox has stopped wroking” problems [many users were reporting](https://github.com/mozilla/geckodriver/issues/225) when deleting a session
- Preferences passed in a profile now take precedence over set of default preferences defined by geckodriver (fixed by [@DrMarcII](https://github.com/DrMarcII))
  - The exceptions are the `marionette.port` and `marionette.log.level` preferences and their fallbacks, which are set unconditionally and cannot be overriden
- Remove default preference that disables unsafe CPOW checks
- WebDriver library updated to 0.25.1

### Fixed
- Fix for the “corrupt deflate stream” exception that sometimes occured when trying to write an empty profile by [@kirhgoph](https://github.com/kirhgoph)

## 0.15.0 (2017-03-08)

### Added
- Added routing and parsing for the [Get Timeouts](https://w3c.github.io/webdriver/webdriver-spec.html#dfn-get-timeouts) command

### Changed
- All HTTP responses are now wrapped in `{value: …}` objects per the WebDriver specification; this may likely require you to update your client library
- Pointer move action’s `element` key changed to `origin`, which lets pointer actions originate within the context of the viewport, the pointer’s current position, or from an element
- Now uses about:blank as the new tab document; this was previously disabled due to [bug 1333736](https://bugzilla.mozilla.org/show_bug.cgi?id=1333736) in Marionette
- WebDriver libary updated to 0.23.0

### Fixed
- Aligned the data structure accepted by the [Set Timeouts](https://w3c.github.io/webdriver/webdriver-spec.html#set-timeouts) command with the WebDriver specification

## 0.14.0 (2017-01-31)

### Changed
- Firefox process is now terminated and session ended when the last window is closed
- WebDriver library updated to version 0.20.0

### Fixed
- Stacktraces are now included when the error originates from within the Rust stack
- HTTPD now returns correct response headers for `Content-Type` and `Cache-Control` thanks to @jugglinmike

## 0.13.0 (2017-01-06)

### Changed
- When navigating to a document with an insecure- or otherwise invalid TLS certificate, an [insecure certificate](https://w3c.github.io/webdriver/webdriver-spec.html#dfn-insecure-certificate) error will be returned
- On macOS, deducing Firefox’ location on the system will look for _firefox-bin_ on the system path (`PATH` environmental variable) before looking in the applications folder
- Window position coordinates are allowed to be negative numbers, to cater for maximised window positioning on Windows
- WebDriver library updated to version 0.18.0

### Fixed
- Check for single-character key codes in action sequences now counts characters instead of bytes

## 0.12.0 (2017-01-03)

### Added
- Added [_Take Element Screenshot_](https://w3c.github.io/webdriver/webdriver-spec.html#take-element-screenshot) command
- Added new [_Status_](https://w3c.github.io/webdriver/webdriver-spec.html#status) command
- Added routing for the [_Get Timeouts_](https://w3c.github.io/webdriver/webdriver-spec.html#get-timeouts) command, but it is not yet implemented in Marionette, and will return an _unsupported operation_ error until it is
- Implemented routing for [new actions API](https://w3c.github.io/webdriver/webdriver-spec.html#actions), but it too is not yet fully implemented in Marionette

### Changed
- [Synced Firefox preferences](https://github.com/mozilla/geckodriver/commit/2bfdc3ec8151c427a6a75a6ba3ad203459540495) with those used in Mozilla automation
- Default log level for debug builds of Firefox, which used to be `DEBUG`, changed to `INFO`-level
- WebDriver library dependency upgraded to 0.17.1
- Using _session not created_ error when failing to start session
- geckodriver will exit with exit code 69 to indicate that the port is unavailable

### Fixed
- Improved logging when starting Firefox
- Reverted to synchronous logging, which should address cases of inconsistent output when failing to bind to port
- Clarified in README that geckodriver is not supported on Windows XP
- Added documentation of supported capabilities to [README](https://github.com/mozilla/geckodriver/blob/master/README.md)
- Included capabilities example in [README](https://github.com/mozilla/geckodriver/blob/master/README.md)

## 0.11.1 (2016-10-10)

### Fixed
- Version number in binary now reflects the release version.

## 0.11.0 (2016-10-10)

### Added
- Introduced continous integration builds for Linux- and Windows 32-bit binaries
- Added commands for setting- and getting the window position
- Added new extension commands for finding an element’s anonymous children and querying its attributes; accessible through the `/session/{sessionId}/moz/xbl/{elementId}/anonymous_children` to return all anonymous children and `/session/{sessionId}/moz/xbl/{elementId}/anonymous_by_attribute` to return an anonymous element by a name and attribute query
- Introduced a `moz:firefoxOptions` capability to customise a Firefox session:
  - The `binary`, `args`, and `profile` entries on this dictionary is equivalent to the old `firefox_binary`, `firefox_args`, and `firefox_profile` capabilities, which have now all been removed
  - The `log` capability takes a dictionary such as `{log: "trace"}` to enable trace level verbosity in Gecko
  - The `prefs` capability lets you define Firefox preferences through capabilities
- Re-introduced the `--webdriver-port` argument as a hidden alias to `--port`

### Changed
- `firefox_binary`, `firefox_args`, and `firefox_profile` capabilities removed in favour of the `moz:firefoxOptions` dictionary detailed above and in the README
- Removed `--no-e10s` flag, and geckodriver will from now rely on the Firefox default multiprocessing settings (override using preferences)
- Disable pop-up blocker in the default profile by @juangj
- Changed Rust compiler version to 1.12 (beta) temporarily because of [trouble linking Musl binaries](https://github.com/rust-lang/rust/issues/34978)
- Replaced _env_logger_ logging facility with the _slog_ package, causing the `RUST_LOG` environment variable to no longer have any affect
- Updated the WebDriver Rust library to version 0.15.

### Fixed
- Corrected link to repository in Cargo metadata
- Verbosity shorthand flag `-v[v]` now works again, following the replacement of the argument parsing library in the previous release
- When the HTTPD fails to start, errors are propagated to the user
- Disabled the additional welcome URL (`startup.homepage_welcome_url.additional`) so that officially branded Firefox builds do not start with two open tabs in fresh profiles
- Disabled homepage override URL redirection on milestone upgrades, which means a tab with an upgrade notice is not displayed when launching a new Firefox version

## 0.10.0 (2016-08-02)

### Changed
- Use multi-process Firefox (e10s) by default, added flag `--no-e10s` to disable it and removed `--e10s` flag
- Disable autofilling of forms by default by @mythsunwind
- Replace _argparse_ with _clap_ for arguments parsing

### Fixed
- Attempt to deploy a single file from Travis when making a release
- Grammar fix in README


## 0.9.0 (2016-06-30)

### Added
- Add ability to use `firefox_binary` capability to define location of Firefox to use
- Automatically detect the default Firefox path if one is not given
- Cross-compile to Windows and ARMv7 (HF) in CI
- Add Musl C library-backed static binaries in CI
- Add `-v`, `-vv`, and `--log LEVEL` flags to increase Gecko verbosity
- Add Get Element Property endpoint
- Add new `--version` flag showing copying information and a link to the repository

### Changed
- Now connects to a Marionette on a random port by default
- Update webdriver-rust library dependency
- Migrated to use Travis to deploy new releases
- Reduced amount of logging
- Introduced a changelog (this)


## 0.8.0 (2016-06-07)

### Added
- Allow specifying array of arguments to the Firefox binary through the `firefox_args` capability
- Pass parameters with New Session command

### Changed
- Change product name to _geckodriver_
- Make README more exhaustive
- Quit Firefox when deleting a session
- Update webdriver-rust library
- Update dependencies

### Fixed
- Fix tests
- FIx typo in error message for parsing errors


## 0.7.1 (2016-04-27)

### Added
- Add command line flag for using e10s enabeld Firefox by @martionsideofthemoon
- Allow providing custom profiels

### Changed
- Allow binding to an IPv6 address by @juangj
- By default, connect to host-agnostic localhost by @juangj
- Make `GeckoContextParameters` public
- Update dependencies

### Fixed
- Squash rustc 1.6 warnings by using `std::thread::sleep(dur: Duration)`


## 0.6.2 (2016-01-20)

### Added
- Add LICENSE file from @joshbruning
- Schedule builds in CI on pushes and pull requests

### Changed
- Enable CPOWs in Marionette


## 0.6.0 (2016-01-12)

### Added
- Add Get Page Source endpoint

### Changed
- Handle arrays being sent from Marionette
- Correct build steps in README
- Update what properties are read from errors sent by Marionette
- Update dependencies


## 0.5.0 (2015-12-10)

### Changed
- Update argparse dependency to use Cargo
- Update to the latest version of the Marionette wire protocol
- Update to latest webdriver-rust library
- Update dependencies


## 0.4.2 (2015-10-02)

### Changed
- Skip compiling optional items in hyper


## 0.4.1 (2015-10-02)

### Changed
- Update webdriver-rust library
- Update dependencies


## 0.4.0 (2015-09-28)

### Added
- Add command extensions for switching between content- and chrome contexts
- Add more documentation from @vladikoff

### Changed
- Update Cargo.lock with new dependencies for building
- Update for protocol updates that flatten commands
- Update to new protocol error handling
- Update for Marionette protocol version 3 changes
- Strip any leading and trailing `{}` from the `sessionId` Marionette returns
- Update dependencies

### Fixed
- Fix `GetCSSValue` message to send correct key `propertyName`
- Fix example in documentation from @vladikoff


## 0.3.0 (2015-08-17)

### Added
- Add support for finding elements in subtrees


## 0.2.0 (2015-05-20)

### Added
- Extra debug messages
- Add ability to set WebDriver port
- Add support for getting the active element
- Add support for `GetCookies` and `DeleteCookie`/`DeleteCookies`
- Add preferences that switch off certain features not required for WebDriver tests

### Changed
- Make failing to communicate with Firefox a fatal error that closes the session
- Shut down session only when loosing connection
- Better handling of missing command line flags
- Poll for connection every 100ms rather than every 100s
- Switch to string-based error codes
- Switch webdriver-rust library dependency to be pulled from git
- Update dependencies

### Fixed
- Handle null id for switching to frame more correctly


## 0.1.0 (2015-04-09)

### Added
- Add proxy for converting WebDriver HTTP protocol to Marionette protocol
- Add endpoints for modal dialogue support
- Allow connecting to a running Firefox instance
- Add explicit Cargo.lock file
- Start Firefox when we get a New Session command
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
- Fix conversion of Find Element/Find Elements responses from Marionette to WebDriver
- Fixed build by updating Cargo.lock with new dependencies for building
- Squash compile warnings
