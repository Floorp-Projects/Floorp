[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [SyncStatusObserver](./index.md)

# SyncStatusObserver

`interface SyncStatusObserver` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Sync.kt#L80)

An interface for consumers that wish to observer "sync lifecycle" events.

### Functions

| Name | Summary |
|---|---|
| [onError](on-error.md) | `abstract fun onError(error: `[`Exception`](https://developer.android.com/reference/java/lang/Exception.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Gets called if sync encounters an error that's worthy of processing by status observers. |
| [onIdle](on-idle.md) | `abstract fun onIdle(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Gets called at the end of a sync, after every configured syncable has been synchronized. |
| [onStarted](on-started.md) | `abstract fun onStarted(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Gets called at the start of a sync, before any configured syncable is synchronized. |

### Inheritors

| Name | Summary |
|---|---|
| [GeneralSyncManager](../../mozilla.components.feature.sync/-general-sync-manager/index.md) | `abstract class GeneralSyncManager : `[`SyncManager`](../-sync-manager/index.md)`, `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`SyncStatusObserver`](./index.md)`>, `[`SyncStatusObserver`](./index.md)<br>A base SyncManager implementation which manages a dispatcher, handles authentication and requests synchronization in the following manner: On authentication AND on store set changes (add or remove)... |
