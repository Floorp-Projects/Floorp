[android-components](../../index.md) / [mozilla.components.feature.sync](../index.md) / [GeneralSyncManager](./index.md)

# GeneralSyncManager

`abstract class GeneralSyncManager : `[`SyncManager`](../../mozilla.components.concept.sync/-sync-manager/index.md)`, `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`SyncStatusObserver`](../../mozilla.components.concept.sync/-sync-status-observer/index.md)`>, `[`SyncStatusObserver`](../../mozilla.components.concept.sync/-sync-status-observer/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/sync/src/main/java/mozilla/components/feature/sync/BackgroundSyncManager.kt#L81)

A base SyncManager implementation which manages a dispatcher, handles authentication and requests
synchronization in the following manner:
On authentication AND on store set changes (add or remove)...

* immediate sync and periodic syncing are requested
On logout...
* periodic sync is requested to stop

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `GeneralSyncManager()`<br>A base SyncManager implementation which manages a dispatcher, handles authentication and requests synchronization in the following manner: On authentication AND on store set changes (add or remove)... |

### Properties

| Name | Summary |
|---|---|
| [logger](logger.md) | `open val logger: `[`Logger`](../../mozilla.components.support.base.log.logger/-logger/index.md) |

### Functions

| Name | Summary |
|---|---|
| [addStore](add-store.md) | `open fun addStore(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Add a store, by [name](../../mozilla.components.concept.sync/-sync-manager/add-store.md#mozilla.components.concept.sync.SyncManager$addStore(kotlin.String)/name), to a set of synchronization stores. Implementation is expected to be able to either access or instantiate concrete [SyncableStore](../../mozilla.components.concept.sync/-syncable-store/index.md) instances given this name. |
| [authenticated](authenticated.md) | `open fun authenticated(account: `[`OAuthAccount`](../../mozilla.components.concept.sync/-o-auth-account/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>An authenticated account is now available. |
| [createDispatcher](create-dispatcher.md) | `abstract fun createDispatcher(stores: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, account: `[`OAuthAccount`](../../mozilla.components.concept.sync/-o-auth-account/index.md)`): `[`SyncDispatcher`](../-sync-dispatcher/index.md) |
| [dispatcherUpdated](dispatcher-updated.md) | `abstract fun dispatcherUpdated(dispatcher: `[`SyncDispatcher`](../-sync-dispatcher/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [isSyncRunning](is-sync-running.md) | `open fun isSyncRunning(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Indicates if synchronization is currently active. |
| [loggedOut](logged-out.md) | `open fun loggedOut(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Previously authenticated account has been logged-out. |
| [onError](on-error.md) | `open fun onError(error: `[`Exception`](https://developer.android.com/reference/java/lang/Exception.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Gets called if sync encounters an error that's worthy of processing by status observers. |
| [onIdle](on-idle.md) | `open fun onIdle(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Gets called at the end of a sync, after every configured syncable has been synchronized. |
| [onStarted](on-started.md) | `open fun onStarted(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Gets called at the start of a sync, before any configured syncable is synchronized. |
| [removeStore](remove-store.md) | `open fun removeStore(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Remove a store previously specified via [addStore](../../mozilla.components.concept.sync/-sync-manager/add-store.md) from a set of synchronization stores. |
| [syncNow](sync-now.md) | `open fun syncNow(startup: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Request an immediate synchronization of all configured stores. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [SYNC_PERIOD](-s-y-n-c_-p-e-r-i-o-d.md) | `const val SYNC_PERIOD: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |
| [SYNC_PERIOD_UNIT](-s-y-n-c_-p-e-r-i-o-d_-u-n-i-t.md) | `val SYNC_PERIOD_UNIT: `[`TimeUnit`](https://developer.android.com/reference/java/util/concurrent/TimeUnit.html) |

### Inheritors

| Name | Summary |
|---|---|
| [BackgroundSyncManager](../-background-sync-manager/index.md) | `class BackgroundSyncManager : `[`GeneralSyncManager`](./index.md)<br>A SyncManager implementation which uses WorkManager APIs to schedule sync tasks. |
