[android-components](../../index.md) / [mozilla.components.feature.sync](../index.md) / [BackgroundSyncManager](./index.md)

# BackgroundSyncManager

`class BackgroundSyncManager : `[`GeneralSyncManager`](../-general-sync-manager/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/sync/src/main/java/mozilla/components/feature/sync/BackgroundSyncManager.kt#L51)

A SyncManager implementation which uses WorkManager APIs to schedule sync tasks.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BackgroundSyncManager(syncScope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>A SyncManager implementation which uses WorkManager APIs to schedule sync tasks. |

### Properties

| Name | Summary |
|---|---|
| [logger](logger.md) | `val logger: `[`Logger`](../../mozilla.components.support.base.log.logger/-logger/index.md) |

### Functions

| Name | Summary |
|---|---|
| [createDispatcher](create-dispatcher.md) | `fun createDispatcher(stores: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, account: `[`OAuthAccount`](../../mozilla.components.concept.sync/-o-auth-account/index.md)`): `[`SyncDispatcher`](../-sync-dispatcher/index.md) |
| [dispatcherUpdated](dispatcher-updated.md) | `fun dispatcherUpdated(dispatcher: `[`SyncDispatcher`](../-sync-dispatcher/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inherited Functions

| Name | Summary |
|---|---|
| [addStore](../-general-sync-manager/add-store.md) | `open fun addStore(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Add a store, by [name](../../mozilla.components.concept.sync/-sync-manager/add-store.md#mozilla.components.concept.sync.SyncManager$addStore(kotlin.String)/name), to a set of synchronization stores. Implementation is expected to be able to either access or instantiate concrete [SyncableStore](../../mozilla.components.concept.sync/-syncable-store/index.md) instances given this name. |
| [authenticated](../-general-sync-manager/authenticated.md) | `open fun authenticated(account: `[`OAuthAccount`](../../mozilla.components.concept.sync/-o-auth-account/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>An authenticated account is now available. |
| [isSyncRunning](../-general-sync-manager/is-sync-running.md) | `open fun isSyncRunning(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Indicates if synchronization is currently active. |
| [loggedOut](../-general-sync-manager/logged-out.md) | `open fun loggedOut(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Previously authenticated account has been logged-out. |
| [onError](../-general-sync-manager/on-error.md) | `open fun onError(error: `[`Exception`](https://developer.android.com/reference/java/lang/Exception.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Gets called if sync encounters an error that's worthy of processing by status observers. |
| [onIdle](../-general-sync-manager/on-idle.md) | `open fun onIdle(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Gets called at the end of a sync, after every configured syncable has been synchronized. |
| [onStarted](../-general-sync-manager/on-started.md) | `open fun onStarted(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Gets called at the start of a sync, before any configured syncable is synchronized. |
| [removeStore](../-general-sync-manager/remove-store.md) | `open fun removeStore(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Remove a store previously specified via [addStore](../../mozilla.components.concept.sync/-sync-manager/add-store.md) from a set of synchronization stores. |
| [syncNow](../-general-sync-manager/sync-now.md) | `open fun syncNow(startup: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Request an immediate synchronization of all configured stores. |
