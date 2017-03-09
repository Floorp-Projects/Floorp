# geckodriver [![Build Status](https://travis-ci.org/mozilla/geckodriver.svg?branch=master)](https://travis-ci.org/mozilla/geckodriver)

Proxy for using W3C WebDriver-compatible clients
to interact with Gecko-based browsers.

This program provides the HTTP API described by
the [WebDriver protocol](http://w3c.github.io/webdriver/webdriver-spec.html#protocol)
to communicate with Gecko browsers, such as Firefox.
It translates calls into
the [Marionette automation protocol](https://developer.mozilla.org/en-US/docs/Mozilla/QA/Marionette)
by acting as a proxy between the local- and remote ends.

You can consult the [change log](https://github.com/mozilla/geckodriver/blob/master/CHANGES.md)
for a record of all notable changes to the program.
[Releases](https://github.com/mozilla/geckodriver/releases)
are made available on GitHub
on [supported platforms](#supported-firefoxen).

## Supported Clients

[Selenium](https://github.com/SeleniumHQ/selenium/releases/tag/selenium-3.3.0) version 3.3 or later is required to use geckodriver.

## Supported Firefoxen

Marionette and geckodriver are not yet feature complete.
This means that they do not yet offer full conformance
with the [WebDriver standard](https://w3c.github.io/webdriver/webdriver-spec.html)
or complete compatibility with [Selenium](http://www.seleniumhq.org/).
You can track the [implementation status](https://developer.mozilla.org/en-US/docs/Mozilla/QA/Marionette/WebDriver/status)
of the latest [Firefox Nightly](http://whattrainisitnow.com/) on
[MDN](https://developer.mozilla.org/).
We also keep track of known
[Marionette](https://github.com/mozilla/geckodriver/issues?q=is%3Aissue+is%3Aopen+label%3Amarionette),
[Selenium](https://github.com/mozilla/geckodriver/issues?q=is%3Aissue+is%3Aopen+label%3Aselenium),
and [specification](https://github.com/mozilla/geckodriver/issues?q=is%3Aissue+is%3Aopen+label%3Aspec)
problems in our
[issue tracker](https://github.com/mozilla/geckodriver/issues).

Support is best in Firefox 48 and onwards,
although generally the more recent the Firefox version,
the better the experience as they have more bug fixes and features.
We strongly advise using the [latest Firefox Nightly](https://nightly.mozilla.org/) with geckodriver,
and want to make it clear that Firefox 47 and earlier is explicitly not supported.
Since Windows XP support in Firefox will be dropped with Firefox 53,
we do not support this platform.

## WebDriver capabilities

geckodriver supports a number of
[WebDriver capabilities](https://w3c.github.io/webdriver/webdriver-spec.html#capabilities):

<table>
 <thead>
  <tr>
   <th>Name
   <th>Type
   <th>Description
  </tr>
 </thead>

 <tr>
  <td><code>proxy</code>
  <td><a href=#proxy-object><code>proxy</code></a> object
  <td>Sets browser proxy settings.
 </tr>

 <tr>
  <td><code>acceptInsecureCerts</code>
  <td>boolean
  <td>Boolean initially set to false,
   indicating the session will not implicitly trust untrusted
   or self-signed TLS certificates on navigation.
 </tr>
</table>

### `proxy` object

<table>
 <thead>
  <tr>
   <th>Name
   <th>Type
   <th>Description
  </tr>
 </thead>

 <tr>
  <td><code>proxyType</code>
  <td>string
  <td>Indicates the type of proxy configuration.
   This value must be one of
   <code>pac</code>,
   <code>noproxy</code>,
   <code>autodetect</code>,
   <code>system</code>,
   or <code>manual</code>.
 </tr>

 <tr>
  <td><code>proxyAutoconfigUrl</code>
  <td>string
  <td>Defines the URL for a proxy auto-config file.
   This property should only be set
   when <code>proxyType</code> is <code>pac</code>.
 </tr>

 <tr>
  <td><code>ftpProxy</code>
  <td>string
  <td>Defines the proxy hostname for FTP traffic.
   Should only be set then the <code>proxyType</code>
   is set to <code>manual</code>.
 </tr>

 <tr>
  <td><code>ftpProxyPort</code>
  <td>number
  <td>Defines the proxy port for FTP traffic.
   This property should only be set
   when <code>proxyType</code> is <code>manual</code>.
 </tr>

 <tr>
  <td><code>httpProxy</code>
  <td>string
  <td>Defines the hostname for HTTP traffic.
   This property should only be set
   when <code>proxyType</code> is <code>manual</code>.
 </tr>

 <tr>
  <td><code>httpProxyPort</code>
  <td>number
  <td>Defines the proxy port for HTTP traffic.
   This property should only be set
   when <code>proxyType</code> is <code>manual</code>.
 </tr>

 <tr>
  <td><code>sslProxy</code>
  <td>string
  <td>Defines the proxy hostname
   for encrypted TLS traffic.
   This property should only be set
   when <code>proxyType</code> is <code>manual</code>.
 </tr>

 <tr>
  <td><code>sslProxyPort</code>
  <td>number
  <td>Defines the proxy port for SSL traffic.
   This property should only be set
   when <code>proxyType</code> is <code>manual</code>.
 </tr>

 <tr>
  <td><code>socksProxy</code>
  <td>string
  <td>Defines the proxy hostname for a SOCKS proxy.
   This property should only be set
   when <code>proxyType</code> is <code>manual</code>.
 </tr>

 <tr>
  <td><code>socksProxyPort</code>
  <td>number
  <td>Defines the proxy port for a SOCKS proxy.
   This property should only be set
   when <code>proxyType</code> is <code>manual</code>.
 </tr>

 <tr>
  <td><code>socksVersion</code>
  <td>number
  <td>Defines the SOCKS proxy version.
   This property should only be set
   when <code>proxyType</code> is <code>manual</code>.
 </tr>

 <tr>
  <td><code>socksUsername</code>
  <td>string
  <td>Defines the username used
   when authenticating with a SOCKS proxy.
   This property should only be set
   when <code>proxyType</code> is <code>manual</code>.
 </tr>

 <tr>
  <td><code>socksPassword</code>
  <td>string
  <td>Defines the password used
   when authenticating with a SOCKS proxy.
   This property should only be set
   when <code>proxyType</code> is <code>manual</code>.
 </tr>
</table>

## Firefox capabilities

geckodriver also supports a capability named `moz:firefoxOptions`
which takes Firefox-specific options.
This must be a dictionary
and may contain any of the following fields:

<table>
 <thead>
  <tr>
   <th>Name
   <th>Type
   <th>Description
  </tr>
 </thead>

 <tr>
  <td><code>binary</code>
  <td>string
  <td>Absolute path of the Firefox binary,
   e.g. <code>/usr/bin/firefox</code>
   or <code>/Applications/Firefox.app/Contents/MacOS/firefox</code>,
   to select which custom browser binary to use.
   If left undefined geckodriver will attempt
   to deduce the default location of Firefox
   on the current system.
 </tr>

 <tr>
  <td><code>args</code>
  <td>array&nbsp;of&nbsp;strings
  <td>Command line arguments to pass to the Firefox binary.
   These must include the leading <code>--</code> where required
   e.g. <code>["--devtools"]</code>.
 </tr>

 <tr>
  <td><code>profile</code>
  <td>string
  <td>Base64-encoded zip of a profile directory
   to use as the profile for the Firefox instance.
   This may be used to e.g. install extensions
   or custom certificates.
 </tr>

 <tr>
  <td><code>log</code>
  <td><a href=#log-object><code>log</code></a>&nbsp;object
  <td>Logging options for Gecko.
 </tr>

 <tr>
  <td><code>prefs</code>
  <td><a href=#prefs-object><code>prefs</code></a>&nbsp;object
  <td>Map of preference name to preference value, which can be a
   string, a boolean or an integer.
 </tr>
</table>

### `log` object

<table>
 <thead>
  <tr>
   <th>Name
   <th>Type
   <th>Description
  </tr>
 </thead>

 <tr>
  <td><code>level</code>
  <td>string
  <td>Set the level of verbosity in Gecko.
   Available levels are <code>trace</code>,
   <code>debug</code>, <code>config</code>,
   <code>info</code>, <code>warn</code>,
   <code>error</code>, and <code>fatal</code>.
   If left undefined the default is <code>info</code>.
 </tr>
</table>

### `prefs` object

<table>
 <thead>
  <tr>
   <th>Name
   <th>Type
   <th>Description
  </tr>
 </thead>

 <tr>
  <td><var>preference name</var>
  <td>string, number, boolean
  <td>One entry per preference to override.
 </tr>
</table>

## Capabilities examples

To select a specific Firefox binary
and run it with a specific command-line flag,
set a preference,
and enable verbose logging:

```js
{
	"moz:firefoxOptions": {
		"binary": "/usr/local/firefox/bin/firefox",
		"args": ["--no-remote"],
		"prefs": {
			"dom.ipc.processCount": 8
		},
		"log": {
			"level": "trace"
		}
	}
}
```

## Usage

Usage steps are [documented on MDN](https://developer.mozilla.org/en-US/docs/Mozilla/QA/Marionette/WebDriver),
but the gist of it is this:

	% geckodriver -b /usr/bin/firefox

Or if you’re on Mac:

	% geckodriver -b /Applications/FirefoxNightly.app/Contents/MacOS/firefox-bin

You may also see all flags and options
available in geckodriver by viewing the help message:

	% geckodriver -h
	geckodriver 0.11.1
	WebDriver implementation for Firefox.

	USAGE:
	    geckodriver [FLAGS] [OPTIONS]

	FLAGS:
	        --connect-existing    Connect to an existing Firefox instance
	    -h, --help                Prints help information
	    -v                        Log level verbosity (-v for debug and -vv for
	                              trace level)
	    -V, --version             Prints version and copying information

	OPTIONS:
	    -b, --binary <BINARY>           Path to the Firefox binary
	        --log <LEVEL>
	            Set Gecko log level [values: fatal, error, warn, info, config,
	            debug, trace]
	        --marionette-port <PORT>
	            Port to use to connect to Gecko (default: random free port)
	        --host <HOST>
	            Host ip to use for WebDriver server (default: 127.0.0.1)
	    -p, --port <PORT>
	            Port to use for WebDriver server (default: 4444)

## Building

geckodriver is written in [Rust](https://www.rust-lang.org/)
and you need the [Rust toolchain](https://rustup.rs/) to compile it.

To build the project for release,
ensure you do a compilation with optimisations:

    % cargo build --release

Or if you want a non-optimised binary for debugging:

    % cargo build
