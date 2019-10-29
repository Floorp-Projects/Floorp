---
layout: default
title: Home
nav_order: 1
summary: GeckoView, a WebView-like component from Mozilla specifically designed for building Android browsers.
tags: [GeckoView,Gecko,mozilla,android,WebView,mobile,mozilla-central]
---

# Welcome to GeckoView

Android offers a built-in WebView, which applications can hook into in order to display web pages within the context of their app. However, Android's WebView is not really intended for building browsers, and hence, many advanced Web APIs are disabled. Furthermore, it is also a moving target: different phones might have different versions of WebView, all of which your app has to support.

That is where GeckoView comes in. GeckoView is:

- **Full-featured**: GeckoView is designed to expose the entire power of the Web to applications, and all that through a straightforward API. Think of it as harnessing the full power of Gecko (the engine that powers Firefox), while its API is WebView-like and easy to use.
- **Suited for apps and browsers**: GeckoView is particularly suited for building mobile browsers, but it can be embedded as a web engine component in any kind of app.
- **Self-Contained**: Because GeckoView is a standalone library that you bundle with your application, you can be confident that the code you test is the code that will actually run.
- **Standards Compliant**: Like Firefox, GeckoView offers excellent support for modern Web standards.

## Using GeckoView

* [Quick Start Guide][1]
* [Usage Documentation][7]

## API Documentation

* [Changelog][2]
* [API][3]

## More information
* [GeckoView Wiki][4]
* [GeckoView Source Code][5]
* [Raise a bug on GeckoView code][8]
* [Raise a documentation bug][6]

[1]:consumer/docs/geckoview-quick-start
[2]:javadoc/mozilla-central/org/mozilla/geckoview/doc-files/CHANGELOG
[3]:javadoc/mozilla-central/index.html
[4]:https://wiki.mozilla.org/Mobile/GeckoView
[5]:https://searchfox.org/mozilla-central/source/mobile/android/geckoview
[6]:https://github.com/mozilla/geckoview/issues
[7]:consumer/docs/index
[8]:https://bugzilla.mozilla.org/enter_bug.cgi?product=GeckoView
