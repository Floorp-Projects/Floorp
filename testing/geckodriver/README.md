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

geckodriver supports a capability named `moz:firefoxOptions` which takes
Firefox-specific preference values. This must be a dictionary and may
contain any of the following fields:

<table>
    <thead>
        <tr>
            <th>Name
            <th>Type
            <th>Default
            <th>Description
        </tr>
    </thead>
    <tr>
        <td><code>binary</code>
        <td><code>string</code>
        <td>Taken from <code>-b</code> argument
          or system default location
        <td>Absolute path of the Firefox binary,
    e.g. <code>/usr/bin/firefox</code> or <code>/Applications/Firefox.app/Contents/MacOS/firefox</code>,
    to select which custom browser binary to use.
    If left undefined geckodriver will attempt
    to deduce the default location of Firefox
    on the current system.
    <tr>
        <td><code>args</code>
        <td><code>Array.&ltstring&gt;</code>
        <td>
        <td>Command line arguments to pass to the Firefox binary.
          These must include the leading <code>--</code> where required
          e.g. <code>["--devtools"]</code>.
    </tr>
    <tr>
        <td><code>profile</code>
        <td><code>string</code>
        <td>New, empty profile
        <td>Base64-encoded zip of a profile directory
          to use as the profile for the Firefox instance.
          This may be used to e.g. install extensions or custom certificates.
    </tr>
    <tr>
        <td><code>log</code>
        <td><a href=#log-options>Log options</a> object
        <td>
        <td>Logging options for Gecko.
    </tr>
    <tr>
        <td><code>prefs</code>
        <td><code>Object&lt;string,&nbsp;(string|boolean|integer)&gt</code>
        <td>
        <td>Map of preference name to preference value, which can be a
            string, a boolean or an integer.
    </tr>
</table>

### Log options

<table>
 <thead>
  <tr>
   <th>Name
   <th>Type
   <th>Default
   <th>Description
  </tr>
 </thead>

 <tr>
  <td><code>level</code>
  <td>String
  <td><code>info</code> with optimised Firefox builds,
   and <code>debug</code> with non-optimised
  <td>Set the level of verbosity in Gecko.
   Available levels are <code>trace</code>,
   <code>debug</code>, <code>config</code>,
   <code>info</code>, <code>warn</code>,
   <code>error</code>, and <code>fatal</code>.
 </tr>
</table>

## Building

geckodriver is written in [Rust](https://www.rust-lang.org/)
and you need the [Rust toolchain](https://rustup.rs/) to compile it.

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
