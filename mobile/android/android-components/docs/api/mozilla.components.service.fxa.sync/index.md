[android-components](../index.md) / [mozilla.components.service.fxa.sync](./index.md)

## Package mozilla.components.service.fxa.sync

### Types

| Name | Summary |
|---|---|
| [GlobalSyncableStoreProvider](-global-syncable-store-provider/index.md) | `object GlobalSyncableStoreProvider`<br>A singleton registry of [SyncableStore](../mozilla.components.concept.sync/-syncable-store/index.md) objects. [WorkManagerSyncDispatcher](-work-manager-sync-dispatcher/index.md) will use this to access configured [SyncableStore](../mozilla.components.concept.sync/-syncable-store/index.md) instances. |
| [StorageSync](-storage-sync/index.md) | `class StorageSync : `[`Observable`](../mozilla.components.support.base.observer/-observable/index.md)`<`[`SyncStatusObserver`](-sync-status-observer/index.md)`>`<br>Orchestrates data synchronization of a set of [SyncableStore](../mozilla.components.concept.sync/-syncable-store/index.md)-s. |
| [SyncDispatcher](-sync-dispatcher/index.md) | `interface SyncDispatcher : `[`Closeable`](https://developer.android.com/reference/java/io/Closeable.html)`, `[`Observable`](../mozilla.components.support.base.observer/-observable/index.md)`<`[`SyncStatusObserver`](-sync-status-observer/index.md)`>`<br>Internal interface to enable testing SyncManager implementations independently from SyncDispatcher. |
| [SyncManager](-sync-manager/index.md) | `abstract class SyncManager`<br>A base sync manager implementation. |
| [SyncStatusObserver](-sync-status-observer/index.md) | `interface SyncStatusObserver`<br>An interface for consumers that wish to observer "sync lifecycle" events. |
| [WorkManagerSyncDispatcher](-work-manager-sync-dispatcher/index.md) | `class WorkManagerSyncDispatcher : `[`SyncDispatcher`](-sync-dispatcher/index.md)`, `[`Observable`](../mozilla.components.support.base.observer/-observable/index.md)`<`[`SyncStatusObserver`](-sync-status-observer/index.md)`>, `[`Closeable`](https://developer.android.com/reference/java/io/Closeable.html) |
| [WorkManagerSyncWorker](-work-manager-sync-worker/index.md) | `class WorkManagerSyncWorker : CoroutineWorker` |
| [WorkersLiveDataObserver](-workers-live-data-observer/index.md) | `object WorkersLiveDataObserver`<br>A singleton wrapper around the the LiveData "forever" observer - i.e. an observer not bound to a lifecycle owner. This observer is always active. We will have different dispatcher instances throughout the lifetime of the app, but always a single LiveData instance. |

### Functions

| Name | Summary |
|---|---|
| [getLastSynced](get-last-synced.md) | `fun getLastSynced(context: <ERROR CLASS>): `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |
| [setLastSynced](set-last-synced.md) | `fun setLastSynced(context: <ERROR CLASS>, ts: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
