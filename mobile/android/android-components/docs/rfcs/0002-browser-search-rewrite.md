---
layout: page
title: Replacing `browser-search` component with state in `browser-store`
permalink: /rfc/0002-search-state-in-browser-store
---

* Start date: 2020-07-07
* RFC PR: [#7655](https://github.com/mozilla-mobile/android-components/pull/7655)

## Summary

Moving all search related state to `BrowserState` and replacing `SearchEngineManager` with a `BrowserStore` middleware that deals with saving and loading state; better satisfying the requirements of [Fenix](https://github.com/mozilla-mobile/fenix) and [Focus](https://github.com/mozilla-mobile/focus-android).

## Motivation

The integration of `browser-search` into [Fenix](https://github.com/mozilla-mobile/fenix) and [Firefox for Fire TV](https://github.com/mozilla-mobile/firefox-tv/) showed that the current API is not enough to satisfy all use cases. Some examples include:

* Fenix supports custom search engines. This can only be achieved with `browser-search` by passing a custom `SearchEngineProvider` to `SearchEngineManager`. Everything else is left to the app.
* `SearchEngineManager` initially only provided synchronous methods for accessing search engines. Later asynchronous methods were added. The result is a hard to understand mix that can cause blocking I/O to happen on the wrong threads.
* The `browser-search` component does not follow our abstraction model of implementing a `concept-search` component. This makes it impossible to switch or customize implementations.
* The wrapper code in Fenix makes it hard to argue about the state that is split over two implementations. In addition to that it makes it hard to add functionality to `SearchEngineManager` since it is not used directly.
* Nowadays Android Components bundles all state in `BrowserState` observable through `BrowserStore` (provided by `browser-state`). On the application side we are using an UI-scoped `Store` (provided by `lib-store`). `SearchEngineManager` creates a secondary source of truth just for search and circumvents this architecture pattern.
* In "Firefox for Fire TV" and "Firefox for Echo Show" we needed to override the default search configuration, adding and replacing loaded search engines. This turned out to be quite complicated and required wrapping multiple classes that deal with loading the search configuration and engines.

## Guide-level explanation

Instead of making `SearchEngine` instances accessible through `SearchEngineManager` via a `browser-search` component, we want to make `SearchEngine(State)` available in `BrowserState`. A middleware on the `BrowserStore` will transparently take care of loading `SearchEngine` instances. Whenever the app dispatches an action to change the list of search engines (e.g. adding a custom search engine) then the middleware will take care of saving the new state to disk.

This will allow the app to use the already proven pattern of observing a store to update UI and makes it easy to mix search state with other state without having to query multiple sources. In addition to that the app no longer needs to take care of the storage and fallbacks (e.g. slow MLS query) since that can be handled completely by the middleware.

Using the `BrowserStore` for state will make the app use asynchronous patterns to observe the state and with that avoid blocking the main thread, as well as force the app to deal with the initial state of having no (default) `SearchEngine` yet.

Since the state will move to `browser-state`, additional functionality (querying suggestions, storage, middleware) can move to `feature-search`. This will make `browser-search` obsolete. With that introducing a `concept-search` is not required. An app can use a custom implementation by replacing the middleware, storage or handling search state completely differently.

## Reference-level explanation

The new implementation will use `browser-state` to model all search state.

Functionality on top of the state (querying suggestions, storage, middleware) will live in `feature-search`. With that `browser-search` will no longer be needed and can be removed after following the [deprecation process](https://mozac.org/contributing/deprecating).

### State

`BrowserState` will get a new `search` property with `SearchState` type that lives in `browser-state`:

```Kotlin
data class BrowserState(
   // ...
   val search: SearchState
)

/**
 * Value type that represents the state of available search engines.
 *
 * @property searchEngines List of loaded search engines.
 * @property defaultSearchEngineId ID of the default search engine.
 */
data class SearchState(
  val searchEngines: List<SearchEngine>,
  val defaultSearchEngineId: String
)
```

`SearchState` will contain all `SearchEngine`s (bundled and custom). Additional extension methods will make it easier to select specific subsets or the default `SearchEngine`.

`SearchEngine` will be turned into a data class and moved to `browser-store`. A `type` property will make custom and provided default search engines distinguishable. Other methods like `buildSearchUrl()` will be implemented as extension methods.

### Storage

There will be two storage classes:

* `SearchEngineStorage`: A persistent storage for `SearchEngine`s. The primary use case is for persisting custom search engines added by users.
* `SearchEngineDefaults`: A provider of a list of default search engines (for the user's region) based on `list.json` and the search plugins currently included in `browser-search`.

Those storage classes will have visibility `internal` and will not be used by the app directly.

All storage access methods will be suspending functions to avoid thread-blocking access:

```Kotlin
internal class SearchEngineStorage {
    suspend fun getSearchEngines() = withContext(Dispatchers.IO) {
      // ...
    }
}
```

### Middleware

A `SearchMiddleware` that will be installed on `BrowserStore` will be responsible for:

* Loading the initial set of search engines (from the two storages above) and adding them to `BrowserState` by dispatching actions on `BrowserStore`.
* Persisting state changes it observes in the specific storage:
  * Adding search engines
  * Removing search engines
  * Changing the default search engine
* Updating search engines at runtime (e.g. region or locale change)

Fallback behavior (e.g. region cannot be determined) will be handled by the middleware.

## Drawbacks

* Reimplementing the functionality of `browser-search` will be a project that will take time. Refactoring consuming apps to read the state from `BrowserStore` will take additional time. From the `browser-session` to `browser-state` migration we know that this process can take a long time. Although the scope of `browser-search` is much smaller.

## Rationale and alternatives

* Initially we tried extending the API of `SearchEngineManager` to add additional asynchronous access methods. But this made the API more complicated and did not solve some of the problems of the API itself. In addition to that keeping the old API functional stopped us from fundamentally refactoring some internals.
* We considered directly upstreaming functionality from Fenix to Android Components. While technically possible, this would only improve maintainability but not address some of the API flaws that using the component in Fenix uncovered.
* We considered creating a new `SearchEngineManager` implementation with an asynchronous API (e.g. `Flow`). But this would still create a second source of truth in addition to `BrowserStore` and will make it complicated to observe and mix state from both sources.

## Prior art

We have implemented similar functionality a lot of times:

* [SearchEngineManager in Fennec](https://searchfox.org/mozilla-esr68/source/mobile/android/base/java/org/mozilla/gecko/search/SearchEngineManager.java)
* [SearchEngineManager in Android Components](https://github.com/mozilla-mobile/android-components/blob/08880314f56d73691b3cd909d5dee199bba4ed0b/components/browser/search/src/main/java/mozilla/components/browser/search/SearchEngineManager.kt#L28)
* Focus had its own implementation of `SearchEngineManager` too, but was migrated to use `browser-search` already.
* [FenixSearchEngineProvider](https://github.com/mozilla-mobile/fenix/blob/master/app/src/main/java/org/mozilla/fenix/components/searchengine/FenixSearchEngineProvider.kt) in Fenix
* [SearchService](https://searchfox.org/mozilla-central/source/toolkit/components/search/SearchService.jsm) in Firefox (desktop)
* [SearchEngines.swift](https://github.com/mozilla-mobile/firefox-ios/blob/main/Client/Frontend/Browser/SearchEngines.swift) in Firefox for iOS

## Unresolved questions

* Do the proposed interfaces satisfy the requirements of current consumers of `browser-search`?
  * [Firefox Preview (Fenix)](https://github.com/mozilla-mobile/fenix)
  * [Firefox Focus/Klar](https://github.com/mozilla-mobile/focus-android)
  * [Firefox Reality](https://github.com/MozillaReality/FirefoxReality)
  * [Firefox for Fire TV](https://github.com/mozilla-mobile/firefox-tv/)
  * [Firefox for Echo Show](https://github.com/mozilla-mobile/firefox-echo-show/)
* Some of the naming (e.g. `SearchEngineDefaults`) is not great yet. But that may be unrelated to the proposed change.
