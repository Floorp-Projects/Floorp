# [Android Components](../../../README.md) > Feature > Session

A component that connects an (concept) engine implementation with the browser session module.
A HistoryTrackingDelegate implementation is also provided, which allows tying together
an engine implementation with a storage module.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-session:{latest-version}"
```

### ThumbnailsFeature

Feature implementation for automatically taking thumbnails of sites. The feature will take a screenshot when the page finishes loading, and will add it to the `Session.thumbnail` property.

```kotlin
    val feature = ThumbnailsFeature(context, engineView, sessionManager)
    lifecycle.addObserver(feature)
```

If the OS is under low memory conditions, the screenshot will be not taken. Ideally, this should be used in conjunction with [SessionManager.onLowMemory](https://github.com/mozilla-mobile/android-components/blob/024e3de456e3b46e9bf6718db9500ecc52da3d29/components/browser/session/src/main/java/mozilla/components/browser/session/SessionManager.kt#L472) to allow free up some `Session.thumbnail` from memory.

   ```kotlin
 // Wherever you implement ComponentCallbacks2
    override fun onTrimMemory(level: Int) {
        sessionManager.onLowMemory()
    }
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
