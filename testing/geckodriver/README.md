# geckodriver [![Build Status](https://travis-ci.org/mozilla/geckodriver.svg?branch=master)](https://travis-ci.org/mozilla/geckodriver)

Proxy for using W3C WebDriver-compatible clients
to interact with Gecko-based browsers.

This program provides the HTTP API described by
the [WebDriver protocol](http://w3c.github.io/webdriver/webdriver-spec.html#protocol)
to communicate with Gecko browsers, such as Firefox.
It translates calls into
the [Marionette](https://developer.mozilla.org/en-US/docs/Mozilla/QA/Marionette)
automation protocol
by acting as a proxy between the local- and remote ends.

You can consult the [change log](https://github.com/mozilla/geckodriver/blob/master/CHANGES.md)
for a record of all notable changes to the program.

## Supported Firefoxen

Marionette and geckodriver are not yet feature complete.
This means it does not yet offer full conformance
with the [WebDriver standard](https://w3c.github.io/webdriver/webdriver-spec.html)
or complete compatibility with [Selenium](http://www.seleniumhq.org/).

You can track the [implementation status](https://developer.mozilla.org/en-US/docs/Mozilla/QA/Marionette/WebDriver/status)
of the latest Firefox Nightly on MDN.
We also keep track of known
[Marionette](https://github.com/mozilla/geckodriver/issues?q=is%3Aissue+is%3Aopen+label%3Amarionette),
[Selenium](https://github.com/mozilla/geckodriver/issues?q=is%3Aissue+is%3Aopen+label%3Aselenium),
and [specification](https://github.com/mozilla/geckodriver/issues?q=is%3Aissue+is%3Aopen+label%3Aspec)
problems in our issue tracker.

Marionette support is best in Firefox 48 and onwards,
although the more recent the Firefox version,
the more bug fixes and features.
**Firefox 47 is explicitly not supported.**

## Firefox capabilities

geckodriver supports a couple of non-standard capabilities
to customise and configure a Firefox session:

<dl>
 <dt><code>firefox_binary</code>
 <dd>Set to full path of the Firefox binary,
  e.g. <i>/usr/bin/firefox</i> or <i>/Applications/Firefox.app/Contents/MacOS/firefox</i>,
  to select which custom browser binary to use.
  If left undefined geckodriver will attempt
  to deduce the default location of Firefox
  on the current system.

 <dt><code>firefox_profile</code>
 <dd>For each session, geckodriver creates a new temporary profile by default.
  To set custom preferences or use an add-on/extension,
  you may want to provide a custom profile.
  A profile can be sent across the wire protocol
  by setting this capability to its path
  on the local filesystem.
</dl>

## Building

geckodriver is written in [Rust](https://www.rust-lang.org/)
and you need the Rust toolchain to compile it.

To build the project for release,
ensure you do a compilation with optimisations:

    % cargo build --release

Or if you want a non-optimised binary for debugging:

    % cargo build
 
## Usage

Usage steps are [documented on MDN](https://developer.mozilla.org/en-US/docs/Mozilla/QA/Marionette/WebDriver),
but the gist of it is this:

    % geckodriver -b /usr/bin/firefox

Or if you’re on Mac:

    % geckodriver -b /Applications/FirefoxNightly.app/Contents/MacOS/firefox-bin
