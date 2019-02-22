# [Android Components](../../../README.md) > Feature > Sync

A component which provides facilities for managing data synchronization.

Provided is an implementation of concept-sync's SyncManager, allowing
background execution of synchronization and easily observing synchronization
state.

Currently, synchronization runs periodically in the background.

## Usage

First, set everything up:
```kotlin
// A SyncableStore instance that we want to be able to synchronize.
private val historyStorage = PlacesHistoryStorage(this)

// Add your SyncableStore instances to the global registry.
GlobalSyncableStoreProvider.configureStore("history" to historyStorage)

// Configure SyncManager with sync scope and stores to synchronize.
private val syncManager = BackgroundSyncManager("https://identity.mozilla.com/apps/oldsync").also {
    // Enable synchronization of store called "history".
    // Internally, it will be accessed via GlobalSyncableStoreProvider.
    it.addStore("history")
}

// Configure the account manager with an instance of a SyncManager.
// This will ensure that sync manager is made aware of auth changes.
private val accountManager by lazy {
    FxaAccountManager(
        this,
        Config.release(CLIENT_ID, REDIRECT_URL),
        arrayOf("profile", "https://identity.mozilla.com/apps/oldsync"),
        syncManager
    )
}
```

Once account manager is in an authenticated state, sync manager will immediately
synchronize, as well as schedule periodic syncing.

It's possible to re-configure which stores are to be synchronized via `addStore` and
`removeStore` calls.

It's also possible to observe sync states and request immediate sync of configured stores:
```kotlin
// Define what we want to do in response to sync status changes.
private val syncObserver = object : SyncStatusObserver {
    override fun onStarted() {
        CoroutineScope(Dispatchers.Main).launch {
            syncStatus?.text = getString(R.string.syncing)
        }
    }

    override fun onIdle() {
        CoroutineScope(Dispatchers.Main).launch {
            syncStatus?.text = getString(R.string.sync_idle)

            val resultTextView: TextView = findViewById(R.id.syncResult)
            // Can access synchronized data now.
        }
    }

    override fun onError(error: Exception?) {
        CoroutineScope(Dispatchers.Main).launch {
            syncStatus?.text = getString(R.string.sync_error, error)
        }
    }
}

// Register a sync status observer with this sync manager.
syncManager.register(syncObserver, owner = this, autoPause = true)

// Kick off a sync in response to some user action. Sync will run in the
// background until it completes, surviving application shutdown if needed.
findViewById<View>(R.id.buttonSyncNow).setOnClickListener {
    syncManager.syncNow()
}
```

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-sync:{latest-version}"
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
