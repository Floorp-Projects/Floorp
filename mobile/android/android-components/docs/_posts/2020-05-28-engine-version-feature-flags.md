---
layout: post
title: "🚩 Engine version dependent feature flags"
date: 2020-05-28 14:00:00 +0200
author: sebastian
---

When integrating a completely new feature, for example into [Firefox Preview](https://github.com/mozilla-mobile/fenix), this feature may only work with the latest version of an engine, e.g. the latest version of `browser-engine-gecko-nightly`. Manually maintaining a feature flag that gradually gets enabled in build variants as the required functionality becomes available in more stable engine versions (`browser-engine-gecko-beta`, `browser-engine-gecko`) is cumbersome, error-prone and potentially a multi-week long process.

To help build feature flags, that need to incorporate the engine version, every `Engine` exposes a `version` property. This property is an instance of [`EngineVersion`](https://mozac.org/api/mozilla.components.concept.engine.utils/-engine-version/), which makes it easy to match against specific engine versions.

## Example

Let's say you are adding a new feature to [Firefox Preview](https://github.com/mozilla-mobile/fenix). This new feature requires brand new functionality that was just introduced in GeckoView Nightly 77.0. Using [`isAtLeast()`](https://mozac.org/api/mozilla.components.concept.engine.utils/-engine-version/is-at-least.html) you can create a feature flag that will enable this feature in all build variants that are using GeckoView 77.0 or higher.

```Kotlin
// Enable feature with GeckoView 77+
val useNewFeature = components.engine.version.isAtLeast(77)
```

This also works for minor and patch versions as well as additional metadata that is appended to the version number.

```Kotlin
// Feature requires GeckoView 77.2 or higher.
val useNewFeature = components.engine.version.isAtLeast(77, 2)
```

If needed you can access the individual parts of the version number manually:

```Kotlin
// For GeckoView (Nightly) 77.0a1
engine.version.major // 77
engine.version.minor // 0
engine.version.patch // 0
engine.version.metadata // a1
```

### GeckoView vs. WebView

Note that for GeckoView versions we are using the `MOZILLA_VERSION` that GeckoView exposes (e.g. `78.0a1`) which can be different from version of the maven dependency (e.g. `78.0.20200528032513`).

In `browser-engine-system`, which is using `WebView`, we are parsing the Chrome version from the [User-Agent](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/User-Agent).

```Kotlin
// Mozilla/5.0 (Linux; Android 10) Build/RPP2.200227.014.A1; wv)
// AppleWebKit/537.36 (KHTML, like Gecko) Version 4.0
// Chrome/82.0.4062.3 Mobile Safari/537.36

// On a device with a WebView with the User-Agent above:
engine.version.major // 82
engine.version.minor // 0
engine.version.patch // 4062
engine.version.metadata // .3
```