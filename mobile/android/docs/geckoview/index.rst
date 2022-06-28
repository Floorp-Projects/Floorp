.. -*- Mode: rst; fill-column: 80; -*-

GeckoView
=========

Android offers a built-in WebView, which applications can hook into in order to display web pages within the context of their app. However, Android's WebView is not really intended for building browsers, and hence, many advanced Web APIs are disabled. Furthermore, it is also a moving target: different phones might have different versions of WebView, all of which your app has to support.

That is where GeckoView comes in. GeckoView is:

- **Full-featured**: GeckoView is designed to expose the entire power of the Web to applications, and all that through a straightforward API. Think of it as harnessing the full power of Gecko (the engine that powers Firefox), while its API is WebView-like and easy to use.
- **Suited for apps and browsers**: GeckoView is particularly suited for building mobile browsers, but it can be embedded as a web engine component in any kind of app.
- **Self-Contained**: Because GeckoView is a standalone library that you bundle with your application, you can be confident that the code you test is the code that will actually run.
- **Standards Compliant**: Like Firefox, GeckoView offers excellent support for modern Web standards.

=============
Documentation
=============

.. toctree::
   :titlesonly:

   consumer/index
   contributor/index
   design/index
   Changelog <https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/doc-files/CHANGELOG>
   API Javadoc <https://mozilla.github.io/geckoview/javadoc/mozilla-central/index.html>

=================
More information
=================

* Talk to us on `Matrix <https://chat.mozilla.org/#/room/#geckoview:mozilla.org>`_
* `GeckoView Wiki <https://wiki.mozilla.org/Mobile/GeckoView>`_
* `GeckoView Source Code <https://searchfox.org/mozilla-central/source/mobile/android/geckoview>`_
* `Raise a bug on GeckoView code <https://bugzilla.mozilla.org/enter_bug.cgi?product=GeckoView>`_
* `Raise a documentation bug <https://github.com/mozilla/geckoview/issues>`_
