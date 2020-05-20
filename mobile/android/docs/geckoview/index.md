---
layout: default
title: Geckoview
nav_order: 1
summary: GeckoView, a WebView-like component from Mozilla specifically designed for building Android browsers.
tags: [GeckoView,Gecko,mozilla,android,WebView,mobile,mozilla-central]
---

Android offers a built-in WebView, which applications can hook into in order to display web pages within the context of their app. However, Android's WebView is not really intended for building browsers, and hence, many advanced Web APIs are disabled. Furthermore, it is also a moving target: different phones might have different versions of WebView, all of which your app has to support.

That is where GeckoView comes in. GeckoView is:

- **Full-featured**: GeckoView is designed to expose the entire power of the Web to applications, and all that through a straightforward API. Think of it as harnessing the full power of Gecko (the engine that powers Firefox), while its API is WebView-like and easy to use.
- **Suited for apps and browsers**: GeckoView is particularly suited for building mobile browsers, but it can be embedded as a web engine component in any kind of app.
- **Self-Contained**: Because GeckoView is a standalone library that you bundle with your application, you can be confident that the code you test is the code that will actually run.
- **Standards Compliant**: Like Firefox, GeckoView offers excellent support for modern Web standards.

## Using GeckoView

* [Quick Start Guide](https://firefox-source-docs.mozilla.org/mobile/android/geckoview/consumer/docs/geckoview-quick-start.html)
* [Usage Documentation](https://firefox-source-docs.mozilla.org/mobile/android/geckoview/consumer/docs/index.html)

## API Documentation

* [Changelog](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/doc-files/CHANGELOG)
* [API](https://firefox-source-docs.mozilla.org/index.html)

## More information
* [GeckoView Wiki][1]
* [GeckoView Source Code][2]
* [Raise a bug on GeckoView code][3]
* [Raise a documentation bug][4]

[1]:https://wiki.mozilla.org/Mobile/GeckoView
[2]:https://searchfox.org/mozilla-central/source/mobile/android/geckoview
[3]:https://bugzilla.mozilla.org/enter_bug.cgi?product=GeckoView
[4]:https://github.com/mozilla/geckoview/issues
