Profiles
========

geckodriver uses [profiles] to instrument Firefox’ behaviour.  The
user will usually rely on geckodriver to generate a temporary,
throwaway profile.  These profiles are deleted when the WebDriver
session expires.

In cases where the user needs to use custom, prepared profiles,
geckodriver will make modifications to the profile that ensures
correct behaviour.  See [_Automation preferences_] below on the
precedence of user-defined preferences in this case.

Custom profiles can be provided two different ways:

  1. by appending `--profile /some/location` to the [`args` capability],
     which will instruct geckodriver to use the profile _in-place_;

  2. or by setting the [`profile` capability] to a Base64-encoded
     ZIP of the profile directory.

Note that geckodriver has a [known bug concerning `--profile`] that
prevents the randomised Marionette port from being passed to
geckodriver.  To circumvent this issue, make sure you specify the
port manually using `--marionette-port <port>`.

The second way is compatible with shipping Firefox profiles across
a network, when for example the geckodriver instance is running on
a remote system.  This is the case when using Selenium’s `RemoteWebDriver`
concept, where the WebDriver client and the server are running on
two distinct systems.

[profiles]: https://support.mozilla.org/en-US/kb/profiles-where-firefox-stores-user-data
[_Automation preferences_]: #automation-preferences
[`args` capability]: ./Capabilities.html#capability-args
[`profile` capability]: ./Capabilities.html#capability-profile
[known bug concerning `--profile`]: https://github.com/mozilla/geckodriver/issues/1058


Default locations for temporary profiles
----------------------------------------

When a custom user profile is not provided with the `-profile`
command-line argument geckodriver generates a temporary, throwaway
profile.  This is written to the default system temporary folder
and subsequently removed when the WebDriver session expires.

The default location for temporary profiles depends on the system.
On Unix systems it uses /tmp, and on Windows it uses the Windows
directory.

The default location can be overridden.  On Unix you set the `TMPDIR`
environment variable.  On Windows, the following environment variables
are respected, in order:

  1. `TMP`
  2. `TEMP`
  3. `USERPROFILE`

It is not necessary to change the temporary directory system-wide.
All you have to do is make sure it gets set for the environment of
the geckodriver process:

	% TMPDIR=/some/location ./geckodriver


Automation preferences
----------------------

As indicated in the introduction, geckodriver configures Firefox
so it is well-behaved in automation environments.  It uses a
combination of preferences written to the profile prior to launching
Firefox (1), and a set of recommended preferences set on startup (2).

These can be perused here:

  1. [testing/geckodriver/src/prefs.rs](https://searchfox.org/mozilla-central/source/testing/geckodriver/src/prefs.rs)
  2. [testing/marionette/components/marionette/marionette.js](https://searchfox.org/mozilla-central/source/testing/marionette/components/marionette.js)

As mentioned, these are _recommended_ preferences, and any user-defined
preferences in the [user.js file] or as part of the [`prefs` capability]
take precedence.  This means for example that the user can tweak
`browser.startup.page` to override the recommended preference for
starting the browser with a blank page.

The recommended preferences set at runtime (see 2 above) may also
be disabled entirely by setting `marionette.prefs.recommended`.
This may however cause geckodriver to not behave correctly according
to the WebDriver standard, so it should be used with caution.

Users should take note that the `marionette.port` preference is
special, and will always be overridden when using geckodriver unless
the `--marionette-port <port>` flag is used specifically to instruct
the Marionette server in Firefox which port to use.

[user.js file]: http://kb.mozillazine.org/User.js_file
[`prefs` capability]: ./Capabilities.html#capability-prefs


Temporary profiles not being removed
------------------------------------

It is a known bug that geckodriver in some instances fails to remove
the temporary profile, particularly when the session is not explicitly
deleted or the process gets interrupted.  See [geckodriver issue
299] for more information.

[geckodriver issue 299]: https://github.com/mozilla/geckodriver/issues/299
