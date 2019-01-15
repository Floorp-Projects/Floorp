geckodriver
===========

Proxy for using W3C [WebDriver] compatible clients to interact with
Gecko-based browsers.

This program provides the HTTP API described by the [WebDriver protocol]
to communicate with Gecko browsers, such as Firefox.  It translates calls
into the [Firefox remote protocol] by acting as a proxy between the local-
and remote ends.

geckodriver’s [source code] is made available under the [Mozilla
Public License].

[WebDriver protocol]: https://w3c.github.io/webdriver/#protocol
[Firefox remote protocol]: https://firefox-source-docs.mozilla.org/testing/marionette/marionette/Protocol.html
[source code]: https://hg.mozilla.org/mozilla-unified/file/tip/testing/geckodriver
[Mozilla Public License]: https://www.mozilla.org/en-US/MPL/2.0/
[WebDriver]: https://developer.mozilla.org/en-US/docs/Web/WebDriver


Downloads
---------

* [Releases](https://github.com/mozilla/geckodriver/releases/latest)
* [Change log](https://searchfox.org/mozilla-central/source/testing/geckodriver/CHANGES.md)


Documentation
-------------

* [WebDriver] (work in progress)
  * [Commands](https://developer.mozilla.org/en-US/docs/Web/WebDriver/Commands)
  * [Errors](https://developer.mozilla.org/en-US/docs/Web/WebDriver/Errors)
  * [Types](https://developer.mozilla.org/en-US/docs/Web/WebDriver/Types)

* [Cross browser testing](https://developer.mozilla.org/en-US/docs/Learn/Tools_and_testing/Cross_browser_testing)

* [Selenium](https://seleniumhq.github.io/docs/) (work in progress)
  * [C# API](https://seleniumhq.github.io/selenium/docs/api/dotnet/)
  * [JavaScript API](https://seleniumhq.github.io/selenium/docs/api/javascript/)
  * [Java API](https://seleniumhq.github.io/selenium/docs/api/java/)
  * [Perl API](https://metacpan.org/pod/Selenium::Remote::Driver)
  * [Python API](https://seleniumhq.github.io/selenium/docs/api/py/)
  * [Ruby API](https://seleniumhq.github.io/selenium/docs/api/rb/)

* [geckodriver usage](https://firefox-source-docs.mozilla.org/testing/geckodriver/geckodriver/Usage.html)
  * [Supported platforms](https://firefox-source-docs.mozilla.org/testing/geckodriver/geckodriver/Support.html)
  * [Firefox capabilities](https://firefox-source-docs.mozilla.org/testing/geckodriver/geckodriver/Capabilities.html)
  * [Capabilities example](https://firefox-source-docs.mozilla.org/testing/geckodriver/geckodriver/Capabilities.html#capabilities-example)
  * [Enabling trace logs](https://firefox-source-docs.mozilla.org/testing/geckodriver/geckodriver/TraceLogs.html)
  * [Analyzing crash data from Firefox](https://firefox-source-docs.mozilla.org/testing/geckodriver/geckodriver/CrashReports.html)

* [Contributing](https://firefox-source-docs.mozilla.org/testing/geckodriver/geckodriver/#for-developers)


Source code
-----------

geckodriver’s canonical source code can be found in [mozilla-central].
We only use this GitHub repository for issue tracking and making releases.
See our [contribution documentation] for more information.

[mozilla-central]: https://hg.mozilla.org/mozilla-central/file/tip/testing/geckodriver
[contribution documentation]: https://firefox-source-docs.mozilla.org/testing/geckodriver/geckodriver/#for-developers


Contact
-------

The mailing list for geckodriver discussion is
tools-marionette@lists.mozilla.org ([subscribe], [archive]).

There is also an IRC channel to talk about using and developing
geckodriver in #interop on irc.mozilla.org.

[subscribe]: https://lists.mozilla.org/listinfo/tools-marionette
[archive]: https://lists.mozilla.org/pipermail/tools-marionette/
