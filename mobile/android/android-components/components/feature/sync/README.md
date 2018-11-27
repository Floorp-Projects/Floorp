# [Android Components](../../../README.md) > Feature > Sync

A component which allows synchronizing groups of similar SyncableStores.
SyncableStores must share an AuthType generic type in order to be grouped.

## Usage

This feature is configured with a CoroutineContext and a set of SyncableStore instances.

Here is an example of how this might look for if we were using it with `services-sync-logins`.

First, set it up:
```kotlin
    val loginsStoreName: String = "placesLogins"
    val loginsStore by lazy {
        SyncableLoginsStore(
            AsyncLoginsStorageAdapter.forDatabase(File(applicationContext.filesDir, "logins.sqlite").canonicalPath)
        ) {
            CompletableDeferred("my-not-so-secret-password")
        }
    }

    val featureSync by lazy {
        val context = Dispatchers.IO + job

        FirefoxSyncFeature(context) { authInfo ->
            SyncUnlockInfo(
                fxaAccessToken = authInfo.fxaAccessToken,
                kid = authInfo.kid,
                syncKey = authInfo.syncKey,
                tokenserverURL = authInfo.tokenServerUrl
            )
        }.also {
            it.addSyncable(loginsStoreName, loginsStore)
        }
    }

```

Then trigger a sync when appropriate:
```kotlin
    val firefoxAccount = getAuthenticatedAccount()

    val syncResult = try {
        featureSync.sync(firefoxAccount).await()
    } catch (e: AuthException) {
        // handle auth exception...
        return
    }

    val loginsSyncStatus = syncResult[loginsStoreName]!!.status
    if (loginsSyncStatus is SyncError) {
        // handle a sync exception
    } else {
        // all good!
    }
```

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-sync:{latest-version}"
```

You will also likely need to pull in a `concept-storage` dependency, which
provides sync result type definitions.

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
