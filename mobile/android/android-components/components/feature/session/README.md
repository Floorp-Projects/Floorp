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

### SwipeRefreshFeature
Sample code can be found in [Sample Browser app](https://github.com/mozilla-mobile/android-components/tree/main/samples/browser).

Class to add pull to refresh functionality to browsers. You should pass it a reference to a [`SwipeRefreshLayout`](https://developer.android.com/reference/kotlin/androidx/swiperefreshlayout/widget/SwipeRefreshLayout.html) and the SessionManager.

Your layout should have a `SwipeRefreshLayout` with an `EngineView` as its only child view.

```xml
<androidx.swiperefreshlayout.widget.SwipeRefreshLayout
    android:id="@+id/swipeRefreshLayout"
    android:layout_width="match_parent"
    android:layout_height="match_parent">
    <mozilla.components.concept.engine.EngineView
        android:id="@+id/engineView"
        android:layout_width="match_parent"
        android:layout_height="match_parent" />
</androidx.swiperefreshlayout.widget.SwipeRefreshLayout>
```

In your fragment code, use `SwipeRefreshFeature` to connect the `SwipeRefreshLayout` with your `SessionManager` and `ReloadUrlUseCase`.

```kotlin
    val feature = BrowserSwipeRefresh(sessionManager, sessionUseCases.reload, swipeRefreshLayout)
    lifecycle.addObserver(feature)
```

`SwipeRefreshFeature` provides its own [`SwipeRefreshLayout.OnChildScrollUpCallback`](https://developer.android.com/reference/kotlin/androidx/swiperefreshlayout/widget/SwipeRefreshLayout.OnChildScrollUpCallback.html) and [`SwipeRefreshLayout.OnRefreshListener`](https://developer.android.com/reference/kotlin/androidx/swiperefreshlayout/widget/SwipeRefreshLayout.OnRefreshListener.html) implementations that you should not override.

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
