geckodriver
===========

Proxy for using W3C [WebDriver] compatible clients to interact with
Gecko-based browsers.

This program provides the HTTP API described by the [WebDriver
protocol] to communicate with Gecko browsers, such as Firefox.  It
translates calls into the [Marionette remote protocol] by acting
as a proxy between the local- and remote ends.

[WebDriver protocol]: https://w3c.github.io/webdriver/#protocol
[Marionette remote protocol]: https://firefox-source-docs.mozilla.org/testing/marionette/
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

* [geckodriver usage](https://firefox-source-docs.mozilla.org/testing/geckodriver/Usage.html)
  * [Supported platforms](https://firefox-source-docs.mozilla.org/testing/geckodriver/Support.html)
  * [Firefox capabilities](https://firefox-source-docs.mozilla.org/testing/geckodriver/Capabilities.html)
  * [Capabilities example](https://firefox-source-docs.mozilla.org/testing/geckodriver/Capabilities.html#capabilities-example)
  * [Enabling trace logs](https://firefox-source-docs.mozilla.org/testing/geckodriver/TraceLogs.html)
  * [Analyzing crash data from Firefox](https://firefox-source-docs.mozilla.org/testing/geckodriver/CrashReports.html)

* [Contributing](https://firefox-source-docs.mozilla.org/testing/geckodriver/#for-developers)
  * [Building](https://firefox-source-docs.mozilla.org/testing/geckodriver/Building.html)
  * [Testing](https://firefox-source-docs.mozilla.org/testing/geckodriver/Testing.html)
  * [Releasing](https://firefox-source-docs.mozilla.org/testing/geckodriver/Releasing.html)
  * [Self-serving an ARM build](https://firefox-source-docs.mozilla.org/testing/geckodriver/ARM.html)


Source code
-----------

geckodriver is made available under the [Mozilla Public License].

Its source code can be found in [mozilla-central] under testing/geckodriver.
This GitHub repository is only used for issue tracking and making releases.

[source code]: https://hg.mozilla.org/mozilla-unified/file/tip/testing/geckodriver
[Mozilla Public License]: https://www.mozilla.org/en-US/MPL/2.0/
[mozilla-central]: https://hg.mozilla.org/mozilla-central/file/tip/testing/geckodriver


Contact
-------

The mailing list for geckodriver discussion is
tools-marionette@lists.mozilla.org ([subscribe], [archive]).

There is also an IRC channel to talk about using and developing
geckodriver in #interop on irc.mozilla.org.

[subscribe]: https://lists.mozilla.org/listinfo/tools-marionette
[archive]: https://lists.mozilla.org/pipermail/tools-marionette/
