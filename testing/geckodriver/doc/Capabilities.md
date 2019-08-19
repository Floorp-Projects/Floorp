Firefox capabilities
====================

geckodriver has a few capabilities that are specific to Firefox.


`moz:firefoxOptions`
--------------------

A dictionary used to define options which control how Firefox gets
started and run. It may contain any of the following fields:

<style type="text/css">
  table { width: 100%; margin-bottom: 2em; }
  table, th, td { border: solid gray 1px; }
  td, th { padding: 5px 10px; }
</style>

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
  <td align="center">string
  <td><p>
   Path to the Firefox executable to select which custom browser binary to use.
   If left undefined geckodriver will attempt
   to deduce the default location of Firefox on the current system.
   If Firefox stable is not installed, it will suggest
   the default location of Firefox Nightly instead.

  <p>
  On macOS the path can either be for the application bundle,
  e.g. <code>/Applications/Firefox.app</code>
  or <code>/Applications/Firefox Nightly.app</code>,
  or point at the executable absolute path inside the bundle,
  e.g. <code>/Applications/Firefox.app/Contents/MacOS/firefox</code>
  or <code>/Applications/Firefox Nightly.app/Contents/MacOS/firefox</code>.
 </tr>

 <tr id=capability-args>
  <td><code>args</code>
  <td align="center">array&nbsp;of&nbsp;strings
  <td><p>Command line arguments to pass to the Firefox binary.
   These must include the leading dash (<code>-</code>) where required,
   e.g. <code>["-devtools"]</code>.

   <p>To have geckodriver pick up an existing profile on the filesystem,
    you may pass <code>["-profile", "/path/to/profile"]</code>.
 </tr>

 <tr id=capability-profile>
  <td><code>profile</code>
  <td align="center">string
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
  <td align="center"><a href=#log-object><code>log</code></a>&nbsp;object
  <td>To increase the logging verbosity of geckodriver and Firefox,
   you may pass a <a href=#log-object><code>log</code> object</a>
   that may look like <code>{"log": {"level": "trace"}}</code>
   to include all trace-level logs and above.
 </tr>

 <tr id=capability-prefs>
  <td><code>prefs</code>
  <td align="center"><a href=#prefs-object><code>prefs</code></a>&nbsp;object
  <td>Map of preference name to preference value, which can be a
   string, a boolean or an integer.
 </tr>
</table>


`moz:useNonSpecCompliantPointerOrigin`
--------------------------------------

A boolean value to indicate how the pointer origin for an action
command will be calculated.

With Firefox 59 the calculation will be based on the requirements by
the [WebDriver] specification. This means that the pointer origin
is no longer computed based on the top and left position of the
referenced element, but on the in-view center point.

To temporarily disable the WebDriver conformant behavior use `false`
as value for this capability.

Please note that this capability exists only temporarily, and that
it will be removed once all Selenium bindings can handle the new behavior.


`moz:webdriverClick`
--------------------

A boolean value to indicate which kind of interactability checks
to run when performing a click or sending keys to an elements. For
Firefoxen prior to version 58.0 some legacy code as imported from
an older version of FirefoxDriver was in use.

With Firefox 58 the interactability checks as required by the
[WebDriver] specification are enabled by default. This means
geckodriver will additionally check if an element is obscured by
another when clicking, and if an element is focusable for sending keys.

Because of this change in behaviour, we are aware that some extra
errors could be returned. In most cases the test in question might
have to be updated so it's conform with the new checks. But if the
problem is located in geckodriver, then please raise an issue in the
[issue tracker].

To temporarily disable the WebDriver conformant checks use `false`
as value for this capability.

Please note that this capability exists only temporarily, and that
it will be removed once the interactability checks have been stabilized.


`log` object
------------

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
  <td align="center">string
  <td>Set the level of verbosity of geckodriver and Firefox.
   Available levels are <code>trace</code>,
   <code>debug</code>, <code>config</code>,
   <code>info</code>, <code>warn</code>,
   <code>error</code>, and <code>fatal</code>.
   If left undefined the default is <code>info</code>.
   The value is treated case-insensitively.
 </tr>
</table>


`prefs` object
--------------

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
  <td align="center">string, number, boolean
  <td>One entry per preference to override.
 </tr>
</table>


Capabilities example
====================

The following example selects a specific Firefox binary to run with
a prepared profile from the filesystem in headless mode (available on
certain systems and recent Firefoxen).  It also increases the number
of IPC processes through a preference and enables more verbose logging.

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