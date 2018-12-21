# [Android Components](../../../README.md) > Feature > Session-Bundling

A session storage implementation that saves the state of sessions in grouped bundles (e.g. by time) in a database.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-session-bundling:{latest-version}"
```

### Setting up

When creating a `SessionBundleStorage` instance provide a bundle lifetime:

```Kotlin
val storage = SessionBundleStorage(
    applicationContext,
    bundleLifetime = Pair(1, TimeUnit.HOURS))
```

The provided lifetime will control how long a session bundle is active. The last session bundle will only be restored if it is not older than the provided lifetime. In case that no recent bundle could be found a new empty session bundle will be created.

### Saving state automatically

An app can call `save(SessionManager.Snapshot)` at any time to save a snapshot in the current session bundle. However in most scenarios it is recommended to setup the automatic saving of state:

```Kotlin
storage.autoSave(sessionManager)
    .periodicallyInForeground() // interval can be configured
    .whenGoingToBackground()    // app goes to the background
    .whenSessionsChange()       // New sessions, pages load, etc.
```

### Restoring state

Calling `restore()` will restore the last active session bundle if it is still active (lifetime has not expired) or null if no active session bundle is available. This bundle can be used to restore the state of the bundle:

```Kotlin
val bundle = storage.restore()

bundle?.restoreSnapshot(engine)?.let { snapshot -> restore(snapshot) }
```

### Working with bundles

The `SessionBundleStorage` class offers multiple methods for accessing and using `SessionBundle` instances:

```Kotlin
// LiveData list of the last 20 bundles
val bundles: LiveData<List<SessionBundle>> = storage.bundles(20)

// DataSource.Factory of all saved bundles. A consuming app can transform the
// data source into a LiveData<PagedList> of when using RxJava2 into a
// Flowable<PagedList> or `Observable<PagedList>`, that can be observed.
// Together with Android's paging library this can be used to create a
// dynamically updating RecyclerView.
val factory: DataSource.Factory<Int, SessionBundle> = storage.pagedBundles()

// Retrieve the bundle currently used for saving the state
val current = storage.current()

// Use a specific bundle for saving state
storage.use(bundle)

// Remove a bundle or all bundles from the storage
storage.remove(bundle)
storage.removeAll()
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
