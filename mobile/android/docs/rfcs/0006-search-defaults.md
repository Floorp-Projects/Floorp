---
layout: page
title: Determining the set of default search engines
permalink: /rfc/0006-search-defaults
---

* Start date: 2020-09-D8
* RFC PR: [#8437](https://github.com/mozilla-mobile/android-components/pull/8437)

## Summary

Determining the set of default search engines based on the user's home region.

## Motivation

Our configuration of default search engines ([list.json](https://github.com/mozilla-mobile/android-components/blob/main/components/browser/search/src/main/assets/search/list.json)) is based on the region and language or locale of the user.

To provide our users with the best choice of search engines and to satisfy the needs of our partnerships, we want to determine (and recognize changes of) the user's region as accurately as possible.

The scope of this document is limited to the resolving and caching of the user's region and using it to determine the default search engines.

### Problems and constraints

* **Fast**: A search engine needs to be available on application start. The search engine icon is used in the application UI and once the user starts typing we need to be able to provide search suggestions and ultimately perform the search.

* **Accurate**: While the locale can be queried from the system, the actual region can be obscured or just harder to determine accurately.

* **Change**: So far, on mobile, we have been doing only a single region lookup. In reality the region of a user can change over time. Especially when the initial installation happened outside of the "home region" of the user then this user remained in the "wrong" region until the app was reinstalled.

* **Privacy**: Not all sources can be accessed at any time. While GPS may be the most accurate source for the user's region, it also requires a runtime permission, that we wouldn't want to ask for on the first app launch.

## Guide-level explanation

### Determining the region

While there are many available sources for determining the region of the user, we will focus on using the Mozilla Location Service (MLS) as the primary source for now. MLS can determine the region of the user by doing a GeoIP lookup. Additionally it supports determining the location based on Bluetooth, cell tower and WiFI access point data. However so far on mobile we have only been using the GeoIP lookup and have not sent any data to MLS.

### Fallback

Other than previously in Fenix we are **not** using the locale as a fallback provider for the region. Instead we will fallback to use the default from the configuration (e.g. `default` in `list.json`).

### Validation

The region returned by MLS may be skewed obscured by proxies, VPNs or inaccuracies in the geo IP databases. To accommodate for this, we want to perform additional validation to verify if the provided region is plausible.

The first _validator_ that we want to ship uses the timezone of the user. Similar to the implementation in the desktop version of Firefox ([[1]](https://searchfox.org/mozilla-central/rev/f82d5c549f046cb64ce5602bfd894b7ae807c8f8/toolkit/modules/Region.jsm#213-224), [[2]](https://searchfox.org/mozilla-central/rev/f82d5c549f046cb64ce5602bfd894b7ae807c8f8/toolkit/modules/Region.jsm#763-779)) users with a `US` region and not a `US` timezone will not get `US` assigned as _current_ or _home_ region.

### Home region and updates

We will use a similar mechanism to track and update the home region of a user as the desktop version of Firefox does.

We will determine between a _home_ region and the _current_ region. Eventually the _current_ region may become the _home_ region of the user. Only the _home_ region will be used for determining the default search engines.

* On the very first app launch we will determine the _current_ region of the user and set it as _home_ region. Until we get a valid response from MLS a user may be on the default search configuration. Since the application usually shows onboarding screens on the first launch, this delay may not be visible to the user.
* On the next app starts we will automatically use the _home_ region. This region is immediately available to the application. Additionally we will query the different providers in the background.
  * If the result matches the _home_ region then no further action is needed.
  * If the result different than the _current_ region then we save the new value as the _current_ region along with the current timestamp.
  * If the result matches the _current_ region and the timestamp of the _current_ region is older than 2 weeks (meaning the _current_ region has not changed in two weeks) then we set the result as the new _home_ region.

With a new _home_ region the user may get a different set of default search engines.

The default search engine of the user will be determined by:

* If the user has not made an active choice: We use the `default` in the configuration (`list.json`) for the _home_ region of the user. When the _home_ region of the uer changes then the default search engine may change too.
* If the user has made an active choice (setting a default in the application's setting) then this search engine will remain the user's default search engine - even if this search engine is not in the list of search engines of the (new) _home_ region of the user.

### Tests

Like for our other components, we want to have a high test coverage for this critical implementation. For the search engine defaults we want to cover some partnership requirements as unit tests to avoid regressions in the affected areas.

## Reference-level explanation

### Region

We will introduce a new (internal) `RegionManager` class that will take care of tracking and updating the _current_ and _home_ region of the user. A `RegionMiddleware` will listen to the `InitAction` of the `BrowserStore` and dispatch an action to add a `RegionState` to the `BrowserState`, as well as asking the `RegionManager` to check whether a region change has happened (by internally querying MLS).

The new `SearchMiddleware` (see [RFC 2 about the new search architecture](https://mozac.org/rfc/0002-search-state-in-browser-store)) will listen to the `RegionState` update, load the matching default search engines using the search storage, and dispatch an action updating the `SearchState`.

## Rationale and alternatives

* An earlier draft of this RFC described using multiple region sources (GNSS, Network and SIM country, Google Play Services) and weighting them. This requires collecting more real world data about the accuracy of those sources and how they drift from what MLS determines, before shipping an implementation and therefore this idea was discarded here. It may be the topic for a separate, future RFC.

## Prior art

* [Timezone check in Firefox desktop](https://searchfox.org/mozilla-central/source/toolkit/modules/Region.jsm#180)
* [Update interval in Firefox desktop](https://searchfox.org/mozilla-central/source/toolkit/modules/Region.jsm#75)
* [RFC about moving state to new AC component](https://mozac.org/rfc/0002-search-state-in-browser-store)
* [Fuzzy location provider idea](https://github.com/mozilla-mobile/android-components/issues/1720)
* [SearchEngineManager in Fennec](https://searchfox.org/mozilla-esr68/source/mobile/android/base/java/org/mozilla/gecko/search/SearchEngineManager.java)
* Current [SearchEngineManager in Android Components](https://github.com/mozilla-mobile/android-components/blob/08880314f56d73691b3cd909d5dee199bba4ed0b/components/browser/search/src/main/java/mozilla/components/browser/search/SearchEngineManager.kt#L28)
* [FenixSearchEngineProvider](https://github.com/mozilla-mobile/fenix/blob/master/app/src/main/java/org/mozilla/fenix/components/searchengine/FenixSearchEngineProvider.kt) in Fenix
* [SearchService](https://searchfox.org/mozilla-central/source/toolkit/components/search/SearchService.jsm) in Firefox (desktop)
* [SearchEngines.swift](https://github.com/mozilla-mobile/firefox-ios/blob/main/Client/Frontend/Browser/SearchEngines.swift) in Firefox for iOS

## Unresolved questions

* What kind of telemetry could or should we collect? What data would help us tweak this? How can we confirm accuracy?

* Is the 2 weeks period before switching the _home_ region, that we borrowed from desktop, acceptable for mobile too?

* Would it help to expose the _current_ and _home_ region in Fenix in a debug screen or in the "about" screen?
