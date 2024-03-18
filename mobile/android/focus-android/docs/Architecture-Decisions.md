This is a collection of records for "architecturally significant" decisions. [Why?](http://thinkrelevance.com/blog/2011/11/15/documenting-architecture-decisions)

---

# Observing state (LiveData)

* **ADR-3**
* Status: Accepted
* Affected version: 2.0+

## Context

During the refactoring of Focus for Android to support multitasking it became apparent that our callback-based approach wasn't good enough anymore. Previously the current browser state was used to update the UI only. Now with multiple concurrent sessions this state needed to be preserved and restored while the user switched between tabs. In addition to that we wanted to update multiple UI components based on the current state (partially) for which a single callback implementation became very complex fast.

Having observers for small chunks of data and a "reactive style" seemed to be a better approach. There are a lot of libraries that offer this functionality already, namely: RxJava, event buses (e.g. Otto and EventBus), Architecture Components (LiveData), Agera and others.

## Decision

LiveData objects from Android's Architecture Components library are used to wrap data that updates asynchronously. UI components that depend on this data can either get the current state or subscribe in order to get updates whenever the data changes.

Reasons:

* The Architecture Components library is written and maintained by the core Android team at Google (-> high quality and visibility)
* Parts of the library are going to be used by the support libraries. Therefore they will get included into Focus for Android sooner or later anyways.
* The library is pretty small.
* The library takes care of the Activity/Fragment lifecycle (Avoids common errors).

## Consequences

* LiveData was designed to be used with `ViewModel` instances. So far we do not use `ViewModel`s and are also observing LiveData objects from non-UI components. This is not problematic but can cause problems in future versions of the library - although unlikely.

---

# Browser engine: GeckoView vs. WebView

* **ADR-2**
* Status: Accepted
* Affected app versions: 1.0+

## Context

To render web content Firefox Focus needs to use a "web browser engine". So far all browsers from Mozilla, and Firefox for Android in particular, are using the [Gecko engine](https://en.wikipedia.org/wiki/Gecko_(software)).

Android itself ships with a [WebView](https://developer.android.com/guide/webapps/webview.html) component that is (on new Android versions) based on Chromium/Chrome ([Blink engine](https://en.wikipedia.org/wiki/Blink_(web_engine))).

A number of existing Android browsers (e.g. Opera, Brave) are built on top of the Blink rendering engine or are a direct fork of Chromium.

## Decision

Firefox Focus is going to be build on top of Android's WebView.

Reasons for using WebView:
* At the time of writing the Focus for Android prototype GeckoView already existed but it wasn't in a state that it could be used outside of Firefox for Android reliably. In addition to that there wasn't a stable API comparable offering the feature set of WebView.
* APK size has been a long-term concern of the Firefox for Android team. A large APK size has been problematic for partnership deals and distribution in countries where bandwidth is limited or expensive. GeckoView is roughly 30 MB in size, while WebView is part of the Android system and is basically "free". Prototype builds of Focus for Android based on WebView were less than 3 MB in size.

In addition to that there will be a build configuration that uses GeckoView. The GeckoView version will be guaranteed to compile; but keeping feature parity or keeping the build bug free is not a goal of the team. At this time the GeckoView version is a tech demo to explore its future potential only.

Forking Chromium (or using Blink) was considered a too large investment in the prototype stage.

## Consequences

WebView has a complex API. Nevertheless it doesn't allow us to do heavy low-level customization that we could do if we would own the web browser engine (e.g. GeckoView). It remains to be seen whether this limitation will prevent feature development in a way that forces us to ship a browser engine with Focus for Android.

---

# Minimum supported Android version: 5.0+ (API 21+)

* **ADR-1**
* Status: Accepted
* Affected app versions: 1.0+

## Context

Every app needs to define a minimum supported SDK version. This is usually a trade-off between how many users can be reached ([Android version distribution](https://developer.android.com/about/dashboards/index.html)) and what platform features the app needs to support ([Android API level overview](https://developer.android.com/guide/topics/manifest/uses-sdk-element.html#ApiLevels)).

## Decision

Focus will support Android versions 5 and higher (API 21+). This decisions is primarily driven by the following platform features that are not available on earlier versions of Android:

* [WebView.shouldInterceptRequest()](shouldInterceptRequest): Our content blocking content uses the implementation of the callback that lets us inspect the request object. This implementation is only available on Android 21+.

* [UI features](https://developer.android.com/training/material/shadows-clipping.html) like `elevation` allow us to build and prototype a "material" UI without needing to backport functionality.

## Consequences

* At the point of writing this ADR we will cover 73.4% of the Android market with that decision [(*)](https://developer.android.com/about/dashboards/index.html).

* Android 4.4 (KitKat) still covers 17.1%. Those users can't install and use Focus. While UI features can be backported, the extended WebView APIs can not. Only a GeckoView based version of Focus would be able to support Android 4.4 or lower.

---
