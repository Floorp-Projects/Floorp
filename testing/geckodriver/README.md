# geckodriver [![Build Status](https://travis-ci.org/mozilla/geckodriver.svg?branch=release)](https://travis-ci.org/mozilla/geckodriver)

Proxy for using W3C WebDriver-compatible clients
to interact with Gecko-based browsers.

This program provides the HTTP API described by the [WebDriver protocol]
to communicate with Gecko browsers, such as Firefox.
It translates calls into the [Firefox remote protocol]
by acting as a proxy between the local- and remote ends.

You can consult the [change log] for a record of all notable changes to the program.
[Releases] are made available on GitHub on [supported platforms].

The canonical source code repository for geckodriver
now lives in [mozilla-central] under [testing/geckodriver].
You can read more about [working with Mozilla source code] on MDN.
This means we do no longer accept pull requests on GitHub.
Patches should be uploaded to a bug
in the [Testing :: GeckoDriver] component.

[WebDriver protocol]: http://w3c.github.io/webdriver/webdriver-spec.html#protocol
[Firefox remote protocol]: https://developer.mozilla.org/en-US/docs/Mozilla/QA/Marionette
[change log]: https://github.com/mozilla/geckodriver/blob/master/CHANGES.md
[Releases]: https://github.com/mozilla/geckodriver/releases
[supported platforms]: #supported-firefoxen
[mozilla-central]: https://hg.mozilla.org/mozilla-central/
[testing/geckodriver]: https://hg.mozilla.org/mozilla-central/file/tip/testing/geckodriver
[working with Mozilla source code]: https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Source_Code
[Testing :: geckodriver]: https://bugzilla.mozilla.org/buglist.cgi?product=Testing&component=geckodriver&resolution=---&list_id=13613952

## Supported clients

[Selenium](http://docs.seleniumhq.org/) users
must update to [version 3.4](https://github.com/SeleniumHQ/selenium/releases/tag/selenium-3.4.0)
or later to use geckodriver.
Other clients that follow the [W3C WebDriver specification](https://w3c.github.io/webdriver/webdriver-spec.html) are also supported.

## Supported Firefoxen

geckodriver is not yet feature complete.
This means that it does not yet offer full conformance
with the [WebDriver standard](https://w3c.github.io/webdriver/webdriver-spec.html)
or complete compatibility with [Selenium](http://www.seleniumhq.org/).
You can track the [implementation status](https://developer.mozilla.org/en-US/docs/Mozilla/QA/Marionette/WebDriver/status)
of the latest [Firefox Nightly](http://whattrainisitnow.com/) on
[MDN](https://developer.mozilla.org/).
We also keep track of known
[Selenium](https://github.com/mozilla/geckodriver/issues?q=is%3Aissue+is%3Aopen+label%3Aselenium),
[remote protocol](https://github.com/mozilla/geckodriver/issues?q=is%3Aissue+is%3Aopen+label%3Amarionette),
and [specification](https://github.com/mozilla/geckodriver/issues?q=is%3Aissue+is%3Aopen+label%3Aspec)
problems in our
[issue tracker](https://github.com/mozilla/geckodriver/issues).

Support is best in Firefox 53 and greater,
although generally the more recent the Firefox version,
the better the experience as they have more bug fixes and features.
Some features will only be available in the most recent Firefox versions,
and we strongly advise using the [latest Firefox Nightly](https://nightly.mozilla.org/) with geckodriver.
Since Windows XP support in Firefox was dropped with Firefox 53,
we do not support this platform.

## WebDriver capabilities

geckodriver supports a number of
[WebDriver capabilities](https://w3c.github.io/webdriver/webdriver-spec.html#capabilities):

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
  <td><code>acceptInsecureCerts</code>
  <td>boolean
  <td>Boolean initially set to false,
   indicating the session will not implicitly trust untrusted
   or self-signed TLS certificates on navigation.
 </tr>

 <tr>
  <td><code>pageLoadStrategy</code>
  <td>string
  <td>Defines the page load strategy
   to use for the duration of the session.
   Setting a page load strategy will cause navigation
   to be "<code>eager</code>",
   waiting for the <code>interactive</code> document ready state;
   "<code>normal</code>" (the default),
   waiting for the <code>complete</code> ready state;
   or "<code>none</code>",
   which will return immediately after starting navigation.
 </tr>

 <tr>
  <td><code>proxy</code>
  <td><a href=#proxy-object><code>proxy</code></a>&nbsp;object
  <td>Sets browser proxy settings.
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
   <code>direct</code>,
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
  <td>Defines the proxy hostname with an optional port for FTP traffic.
   This property should only be set when <code>proxyType</code>
   is set to <code>manual</code>.
 </tr>

 <tr>
  <td><code>httpProxy</code>
  <td>string
  <td>Defines the proxy hostname with an optional port for HTTP traffic.
   This property should only be set when <code>proxyType</code>
   is set to <code>manual</code>.
 </tr>

 <tr>
  <td><code>sslProxy</code>
  <td>string
  <td>Defines the proxy hostname with an optional port for encrypted TLS traffic.
   This property should only be set when <code>proxyType</code>
   is set to <code>manual</code>.
 </tr>

 <tr>
  <td><code>socksProxy</code>
  <td>string
  <td>Defines the hostname with on optional port for a SOCKS proxy.
   This property should only be set when <code>proxyType</code>
   is set to <code>manual</code>.
 </tr>

 <tr>
  <td><code>socksVersion</code>
  <td>number
  <td>Defines the SOCKS proxy version. This property has only to be set
   when <code>proxyType</code> is set to <code>manual</code>.
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

 <tr id=capability-binary>
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

 <tr id=capability-args>
  <td><code>args</code>
  <td>array&nbsp;of&nbsp;strings
  <td><p>Command line arguments to pass to the Firefox binary.
   These must include the leading dash (<code>-</code>) where required,
   e.g. <code>["-devtools"]</code>.

   <p>To have geckodriver pick up an existing profile on the filesystem,
    you may pass <code>["-profile", "/path/to/profile"]</code>.
 </tr>

 <tr id=capability-profile>
  <td><code>profile</code>
  <td>string
  <td><p>Base64-encoded ZIP of a profile directory to use for the Firefox instance.
   This may be used to e.g. install extensions or custom certificates,
   but for setting custom preferences
   we recommend using the <a href=#capability-prefs><code>prefs</code></a> entry
   instead of passing a profile.

   <p>Profiles are created in the system’s temporary folder.
    This is also where the encoded profile is extracted
    when <code>profile</code> is provided.
    By default, geckodriver will create a new profile in this location.

   <p>The effective profile in use by the WebDriver session
    is returned to the user in the <code>moz:profile</code> capability
    in the new session response.

   <p>To have geckodriver pick up an <em>existing profile</em> on the filesystem,
    please set the <a href=#capability-args><code>args</code></a> field
    to <code>{"args": ["-profile", "/path/to/your/profile"]}</code>.
 </tr>

 <tr id=capability-log>
  <td><code>log</code>
  <td><a href=#log-object><code>log</code></a>&nbsp;object
  <td>To increase the logging verbosity of geckodriver and Firefox,
   you may pass a <a href=#log-object><code>log</code> object</a>
   that may look like <code>{"log": {"level": "trace"}}</code>
   to include all trace-level logs and above.
 </tr>

 <tr id=capability-prefs>
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
  <td>Set the level of verbosity of geckodriver and Firefox.
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

## Capabilities example

The following example selects a specific Firefox binary
to run with a prepared profile from the filesystem
in headless mode (available on certain systems and recent Firefoxen).
It also increases the number of IPC processes through a preference
and enables more verbose logging.

```js
{
	"capabilities": {
		"alwaysMatch": {
			"moz:firefoxOptions": {
				"binary": "/usr/local/firefox/bin/firefox",
				"args": ["-headless", "-profile", "/path/to/my/profile"],
				"prefs": {
					"dom.ipc.processCount": 8
				},
				"log": {
					"level": "trace"
				}
			}
		}
	}
}
```

## Usage

Usage steps are [documented on MDN](https://developer.mozilla.org/en-US/docs/Mozilla/QA/Marionette/WebDriver),
but how you invoke geckodriver largely depends on your use case.

### Selenium

If you are using geckodriver through [Selenium](http://seleniumhq.org/),
you must ensure that you have version 3.4 and greater.
Because geckodriver implements the [W3C WebDriver standard](https://w3c.github.io/webdriver/webdriver-spec.html)
and not the same Selenium wire protocol older drivers are using,
you may experience incompatibilities and migration problems
when making the switch from FirefoxDriver to geckodriver.

Generally speaking, Selenium 3 enabled geckodriver
as the default WebDriver implementation for Firefox.
With the release of Firefox 47, FirefoxDriver had to be discontinued
for its lack of support for the [new multi-processing architecture in Gecko](https://developer.mozilla.org/en-US/Firefox/Multiprocess_Firefox).

Selenium client bindings will pick up the _geckodriver_ binary executable
from your [system’s `PATH` environmental variable](https://en.wikipedia.org/wiki/PATH_(variable))
unless you override it by setting the `webdriver.gecko.driver`
[Java VM system property](http://docs.oracle.com/javase/tutorial/essential/environment/sysprop.html):

```java
System.setProperty("webdriver.gecko.driver", "/home/user/bin");
```

Or by passing it as a flag to the [java(1)](http://www.manpagez.com/man/1/java/) launcher:

	% java -Dwebdriver.gecko.driver=/home/user/bin YourApplication

Your milage with this approach may vary
based on which programming language bindings you are using.
It is in any case generally the case that geckodriver will be picked up
if it is available on the system path.
In a bash compatible shell,
you can make other programs aware of its location
by exporting or setting the `PATH` variable:

	% export PATH=$PATH:/home/user/bin
	% whereis geckodriver
	geckodriver: /home/user/bin/geckodriver

On Window systems you can change the system path
by right-clicking **My Computer** and choosing **Properties**.
In the dialogue that appears, navigate
**Advanced** → **Environmental Variables** → **Path**.

Or in the Windows console window:

	$ set PATH=%PATH%;C:\bin\geckodriver

### Standalone

Since geckodriver is a separate HTTP server
that is a complete remote end implementation
of [WebDriver](https://w3c.github.io/webdriver/webdriver-spec.html),
it is possible to avoid using the Selenium remote server
if you have no requirements
to distribute processes across a matrix of systems.

Given a W3C WebDriver conforming client library (or _local end_)
you may interact with the geckodriver HTTP server
as if you were speaking to any Selenium server.

Using [curl(1)](http://www.manpagez.com/man/1/curl/):

	% geckodriver &
	[1] 16010
	% 1491834109194   geckodriver     INFO    Listening on 127.0.0.1:4444
	% curl -d '{"capabilities": {"alwaysMatch": {"acceptInsecureCerts": true}}}' http://localhost:4444/session
	{"sessionId":"d4605710-5a4e-4d64-a52a-778bb0c31e00","value":{"XULappId":"{ec8030f7-c20a-464f-9b0e-13a3a9e97384}","acceptSslCerts":false,"appBuildId":"20160913030425","browserName":"firefox","browserVersion":"51.0a1","command_id":1,"platform":"LINUX","platformName":"linux","platformVersion":"4.9.0-1-amd64","processId":17474,"proxy":{},"raisesAccessibilityExceptions":false,"rotatable":false,"specificationLevel":0,"takesElementScreenshot":true,"takesScreenshot":true,"version":"51.0a1"}}
	% curl -d '{"url": "https://mozilla.org"}' http://localhost:4444/session/d4605710-5a4e-4d64-a52a-778bb0c31e00/url
	{}
	% curl http://localhost:4444/session/d4605710-5a4e-4d64-a52a-778bb0c31e00/url
	{"value":"https://www.mozilla.org/en-US/"
	% curl -X DELETE http://localhost:4444/session/d4605710-5a4e-4d64-a52a-778bb0c31e00
	{}
	% fg
	geckodriver
	^C
	%

Using the Python [wdclient](https://github.com/w3c/wpt-tools/tree/master/webdriver) library:

```py
import webdriver

with webdriver.Session("127.0.0.1", 4444) as session:
    session.url = "https://mozilla.org"
    print "The current URL is %s" % session.url
```

And to run:

	% geckodriver &
	[1] 16054
	% python example.py
	1491835308354   geckodriver     INFO    Listening on 127.0.0.1:4444
	The current URL is https://www.mozilla.org/en-US/
	% fg
	geckodriver
	^C
	%

## Flags

#### <code>-b <var>BINARY</var></code>/<code>--binary <var>BINARY</var></code>

Path to the Firefox binary to use.
By default geckodriver tries to find and use
the system installation of Firefox,
but that behaviour can be changed by using this option.
Note that the `binary` capability of the `moz:firefoxOptions` object
that is passed when [creating a new session](https://w3c.github.io/webdriver/webdriver-spec.html#new-session)
will override this option.

On Linux systems it will use the first _firefox_ binary
found by searching the `PATH` environmental variable,
which is roughly equivalent to calling [whereis(1)](http://www.manpagez.com/man/1/whereis/)
and extracting the second column:

	% whereis firefox
	firefox: /usr/bin/firefox /usr/local/firefox

On macOS, the binary is found by looking for the first _firefox-bin_ binary
in the same fashion as on Linux systems.
This means it is possible to also use `PATH`
to control where geckodriver should find Firefox on macOS.
It will then look for _/Applications/Firefox.app_.

On Windows systems, geckodriver looks for the system Firefox
by scanning the Windows registry.

#### `--connect-existing`

Connecting to an existing Firefox instance.
The instance must have Marionette enabled.

To enable the Marionette remote protocol
you can pass the `--marionette` flag to Firefox,
or (in Firefox 54 or greater)
flip the `marionette.enabled` preference in _about:config_ at runtime.

#### <code>--host <var>HOST</var></code>

Host to use for the WebDriver server.
Defaults to 127.0.0.1.

#### <code>--log <var>LEVEL</var></code>

Set the Gecko and geckodriver log level.
Possible values are `fatal`, `error`, `warn`, `info`, `config`, `debug`, and `trace`.

#### <code>--marionette-port <var>PORT</var></code>

Port to use for connecting to the Marionette remote protocol.
By default it will pick a free port assigned by the system.

#### <code>-p <var>PORT</var></code>/<code>--port <var>PORT</var></code>

Port to use for the WebDriver server.
Defaults to 4444.

A helpful trick is that it is possible to bind to 0
to get the system to atomically assign a free port.

#### <code>-v<var>[v]</var></code>

Increases the logging verbosity by to debug level when passing a single `-v`,
or to trace level if `-vv` is passed.
This is analogous to passing `--log debug` and `--log trace`, respectively.

## Building

geckodriver is written in [Rust], a systems programming language from [Mozilla].
Crucially, it relies on the [webdriver crate] to provide the HTTPD
and do most of the heavy lifting of marshalling the WebDriver protocol.
geckodriver translates WebDriver [commands], [responses], and [errors]
to the [Marionette protocol],
and acts as a proxy between [WebDriver] and [Marionette].

geckodriver is built in the [Firefox CI] by default
but _not_ if you build Firefox locally.
To enable building of geckodriver locally,
ensure you put this in your [mozconfig]:

	ac_add_options --enable-geckodriver

The _geckodriver_ binary will appear in `${objdir}/dist/bin/geckodriver`
alongside _firefox-bin_.

[Rust]: https://www.rust-lang.org/
[Mozilla]: https://www.mozilla.org/en-US/
[webdriver crate]: https://github.com/mozilla/webdriver-rust
[commands]: https://docs.rs/webdriver/0.25.0/webdriver/command/index.html
[responses]: https://docs.rs/webdriver/0.25.0/webdriver/response/index.html
[errors]: https://docs.rs/webdriver/0.25.0/webdriver/error/enum.ErrorStatus.html
[Marionette protocol]: https://developer.mozilla.org/en-US/docs/Mozilla/QA/Marionette/Protocol
[WebDriver]: https://w3c.github.io/webdriver/webdriver-spec.html
[Marionette]: http://searchfox.org/mozilla-central/source/testing/marionette/README
[Firefox CI]: https://treeherder.mozilla.org/
[mozconfig]: https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Build_Instructions/Configuring_Build_Options
