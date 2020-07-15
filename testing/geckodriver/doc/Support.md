Supported platforms
===================

The following table shows a mapping between [geckodriver releases],
and required versions of Selenium and Firefox:

<style type="text/css">
  table { width: 100%; margin-bottom: 2em; }
  table, th, td { border: solid gray 1px; }
  td, th { padding: 5px 10px; text-align: center; }
</style>

<table>
 <thead>
  <tr>
    <th rowspan="2">geckodriver
    <th rowspan="2">Selenium
    <th colspan="2">Firefox
  </tr>
  <tr>
    <th>min
    <th>max
  </tr>
 </thead>

 <tr>
  <td>0.26.0
  <td>≥ 3.11 (3.14 Python)
  <td>60
  <td>n/a
 <tr>
  <td>0.25.0
  <td>≥ 3.11 (3.14 Python)
  <td>57
  <td>n/a
 <tr>
  <td>0.24.0
  <td>≥ 3.11 (3.14 Python)
  <td>57
  <td>79
 <tr>
  <td>0.23.0
  <td>≥ 3.11 (3.14 Python)
  <td>57
  <td>79
 <tr>
  <td>0.22.0
  <td>≥ 3.11 (3.14 Python)
  <td>57
  <td>79
 <tr>
  <td>0.21.0
  <td>≥ 3.11 (3.14 Python)
  <td>57
  <td>79
 <tr>
  <td>0.20.1
  <td>≥ 3.5
  <td>55
  <td>62
 <tr>
  <td>0.20.0
  <td>≥ 3.5
  <td>55
  <td>62
 <tr>
  <td>0.19.1
  <td>≥ 3.5
  <td>55
  <td>62
 <tr>
  <td>0.19.0
  <td>≥ 3.5
  <td>55
  <td>62
 <tr>
  <td>0.18.0
  <td>≥ 3.4
  <td>53
  <td>62
 <tr>
  <td>0.17.0
  <td>≥ 3.4
  <td>52
  <td>62
</table>

Clients
-------

[Selenium] users must update to version 3.11 or later to use geckodriver.
Other clients that follow the [W3C WebDriver specification][WebDriver]
are also supported.

Firefoxen
---------

geckodriver is not yet feature complete.  This means that it does
not yet offer full conformance with the [WebDriver] standard
or complete compatibility with [Selenium].  You can track the
[implementation status] of the latest [Firefox Nightly] on MDN.
We also keep track of known [Selenium], [remote protocol], and
[specification] problems in our [issue tracker].

Support is best in Firefox 57 and greater, although generally the more
recent the Firefox version, the better the experience as they have
more bug fixes and features.  Some features will only be available
in the most recent Firefox versions, and we strongly advise using the
latest [Firefox Nightly] with geckodriver.  Since Windows XP support
in Firefox was dropped with Firefox 53, we do not support this platform.

Android
-------

Starting with the 0.26.0 release geckodriver is able to connect
to Android devices, and to control packages which are based on [GeckoView]
(eg. [Firefox Preview] aka Fenix, or [Firefox Reality]). But it also still
supports versions of Fennec up to 68 ESR, which is the last officially
supported release from Mozilla.

To run tests on Android specific capabilities under `moz:firefoxOptions`
have to be set when requesting a new session. See the Android section under
[Firefox Capabilities](Capabilities.html#android) for more details.

[geckodriver releases]: https://github.com/mozilla/geckodriver/releases
[Selenium]: https://github.com/seleniumhq/selenium
[WebDriver]: https://w3c.github.io/webdriver/
[implementation status]: https://bugzilla.mozilla.org/showdependencytree.cgi?id=721859&hide_resolved=1
[Firefox Nightly]: https://whattrainisitnow.com/
[remote protocol]: https://github.com/mozilla/geckodriver/issues?q=is%3Aissue+is%3Aopen+label%3Amarionette
[specification]: https://github.com/mozilla/geckodriver/issues?q=is%3Aissue+is%3Aopen+label%3Aspec
[issue tracker]: https://github.com/mozilla/geckodriver/issues
[Firefox Nightly]: https://nightly.mozilla.org/
[GeckoView]: https://wiki.mozilla.org/Mobile/GeckoView
[Firefox Preview]: https://play.google.com/store/apps/details?id=org.mozilla.fenix
[Firefox Reality]: https://play.google.com/store/apps/details?id=org.mozilla.vrbrowser
