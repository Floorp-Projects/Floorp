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
