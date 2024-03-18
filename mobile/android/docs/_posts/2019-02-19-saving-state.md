---
layout: post
title: "💾 Saving and restoring browser session state"
date: 2019-02-18 14:35:00 +0200
author: sebastian
---

Losing open tabs in a browser can be a painful experience for the user. By itself [SessionManager](https://mozac.org/api/mozilla.components.browser.session/-session-manager/) keeps its state only in memory. This means that something as simple as switching to a different app can cause Android to [kill the browser app's process and reclaim its resources](https://developer.android.com/guide/components/activities/process-lifecycle). The result of that: The next time the user switches back to the browser app they start with a fresh browser without any tabs open.

[Mozilla's Android Components](https://mozac.org/) come with two implementations of session storage and helpers to write your own easily.

## SessionStorage

The [SessionStorage](https://mozac.org/api/mozilla.components.browser.session.storage/-session-storage/) class that comes with the [browser-session](https://github.com/mozilla-mobile/android-components/tree/main/components/browser/session) component saves the state as a file on disk (using [AtomicFile](https://developer.android.com/reference/android/util/AtomicFile) under the hood). It can be used for a browser that wants to have a single state that gets saved and restored (like Fennec or Chrome).

```kotlin
val sessionStorage SessionStorage(applicationContext, engine)

val sessionManager = sessionManager(engine).apply {
    sessionStorage.restore()?.let { snapshot -> restore(snapshot) }
}
```

ℹ️ Since restoring the last state requires a disk read, it is recommended to perform it off the main thread. This requires the app to gracefully handle the situation where the app starts without any sessions at first. [SessionManager](https://mozac.org/api/mozilla.components.browser.session/-session-manager/) will invoke [onSessionsRestored()](https://mozac.org/api/mozilla.components.browser.session/-session-manager/-observer/on-sessions-restored.html) on a registered [SessionManager.Observer](https://mozac.org/api/mozilla.components.browser.session/-session-manager/-observer/) after restoring has completed.

## SessionBundleStorage

Other than [SessionStorage](https://mozac.org/api/mozilla.components.browser.session.storage/-session-storage/) the [SessionBundleStorage](https://mozac.org/api/mozilla.components.feature.session.bundling/-session-bundle-storage/) implementation can save and restore from multiple states. State is saved as a Bundle in a database.

The storage is set up with a bundle lifetime. [SessionBundleStorage](https://mozac.org/api/mozilla.components.feature.session.bundling/-session-bundle-storage/) will only restore the last bundle if its lifetime has not expired. If there's no active bundle then a new empty bundle will be created to save the state.

```kotlin
val engine: Engine = ...

val sessionStorage = SessionBundleStorage(
    applicationContext,
    bundleLifetime = Pair(1, TimeUnit.HOURS)

val sessionManager = sessionManager(engine).apply {
    // We launch a coroutine on the main thread. Once a snapshot has been restored
    // we want to continue with it on the main thread.
    GlobalScope.launch(Dispatchers.Main) {
        // We restore on the IO dispatcher to not block the main thread:
        val snapshot = async(Dispatchers.IO) {
            val bundle = sessionStorage.restore()
            // If we got a bundle then restore the snapshot from it
            bundle.restoreSnapshot(engine)
        }

        // If we got a snapshot then restore it now:
        snapshot.await()?.let { sessionManager.restore(it) }
    }
}
```

The storage comes with an [API](https://mozac.org/api/mozilla.components.feature.session.bundling/-session-bundle-storage/#functions) that allows apps to build UIs to list, restore, save and remove bundles.

![](/assets/images/blog/session-bundles.png)

## AutoSave

Knowing when to save state, by calling [SessionStorage.save()](https://mozac.org/api/mozilla.components.browser.session.storage/-session-storage/save.html) or [SessionBundleStorage.save()](https://mozac.org/api/mozilla.components.feature.session.bundling/-session-bundle-storage/save.html), can be complicated. Restoring an outdated state can be an as bad a user experience as restoring no state at all.

The [AutoSave](https://mozac.org/api/mozilla.components.browser.session.storage/-auto-save/) class is a helper for configuring automatic saving of the browser state - and you can use it with [SessionStorage](https://mozac.org/api/mozilla.components.browser.session.storage/-session-storage/) as well as [SessionBundleStorage](https://mozac.org/api/mozilla.components.feature.session.bundling/-session-bundle-storage/).

```kotlin
sessionStorage.autoSave(sessionManager)
    // Automatically save the state every 30 seconds:
    .periodicallyInForeground(interval = 30, unit = TimeUnit.SECONDS)
    // Save the state when the app goes to the background:
    .whenGoingToBackground()
    // Save the state whenever sessions change (e.g. a new tab got added or a website
    // has completed loading).
    .whenSessionsChange()
```

## Implementing your own SessionStorage

If neither [SessionStorage](https://mozac.org/api/mozilla.components.browser.session.storage/-session-storage/) nor [SessionBundleStorage](https://mozac.org/api/mozilla.components.feature.session.bundling/-session-bundle-storage/) satisfy the requirements of your app (e.g. you want to save the state in your favorite database or in a cloud-enabled storage) then it is possible to implement a custom storage.

The [AutoSave.Storage](https://mozac.org/api/mozilla.components.browser.session.storage/-auto-save/-storage/) interface from the [browser-session](https://github.com/mozilla-mobile/android-components/tree/main/components/browser/session) component defines the methods that are expected from a session storage. Technically it is not required to implement the interface if your app code is the only one interacting with the session store; but implementing the interface makes your implementation compatible with other components code. Specifically you can use [AutoSave](https://mozac.org/api/mozilla.components.browser.session.storage/-auto-save/) with any class implementing SessionStorage without any additional code.

The [SnapshotSerializer](https://mozac.org/api/mozilla.components.browser.session.storage/-snapshot-serializer/) class is helpful when translating snapshots from and to JSON.

```kotlin
class MyCustomStorage(
    private val engine: Engine
) : AutoSave.Storage {
    private val serializer = SnapshotSerializer()

    override fun save(snapshot: SessionManager.Snapshot): Boolean {
        val json = serializer.toJSON(snapshot)

        // TODO: Save JSON custom storage.

        // Signal that save operation was successful:
        return true
    }

    fun restore(): SessionManager.Snapshot {
        // TODO: Get JSON from custom storage.
        val json = ...

        return serializer.fromJSON(engine, json)
    }
}
```

ℹ️ For simplicity the implementation above does not handle [JSONException](ad https://developer.android.com/reference/org/json/JSONException.html) which can be thrown by [SnapshotSerializer](https://mozac.org/api/mozilla.components.browser.session.storage/-snapshot-serializer/).
