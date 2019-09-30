[android-components](../index.md) / [mozilla.components.service.fxa.sync](./index.md)

## Package mozilla.components.service.fxa.sync

### Types

| Name | Summary |
|---|---|
| [GlobalSyncableStoreProvider](-global-syncable-store-provider/index.md) | `object GlobalSyncableStoreProvider`<br>A singleton registry of [SyncableStore](../mozilla.components.concept.sync/-syncable-store/index.md) objects. [WorkManagerSyncDispatcher](-work-manager-sync-dispatcher/index.md) will use this to access configured [SyncableStore](../mozilla.components.concept.sync/-syncable-store/index.md) instances. |
| [StorageSync](-storage-sync/index.md) | `class StorageSync : `[`Observable`](../mozilla.components.support.base.observer/-observable/index.md)`<`[`SyncStatusObserver`](-sync-status-observer/index.md)`>`<br>Orchestrates data synchronization of a set of [SyncableStore](../mozilla.components.concept.sync/-syncable-store/index.md)-s. |
| [SyncManager](-sync-manager/index.md) | `abstract class SyncManager`<br>A base sync manager implementation. |
| [SyncReason](-sync-reason/index.md) | `sealed class SyncReason`<br>A collection of objects describing a reason for running a sync. |
| [SyncStatusObserver](-sync-status-observer/index.md) | `interface SyncStatusObserver`<br>An interface for consumers that wish to observer "sync lifecycle" events. |
| [WorkManagerSyncDispatcher](-work-manager-sync-dispatcher/index.md) | `class WorkManagerSyncDispatcher : SyncDispatcher, `[`Observable`](../mozilla.components.support.base.observer/-observable/index.md)`<`[`SyncStatusObserver`](-sync-status-observer/index.md)`>, `[`Closeable`](https://developer.android.com/reference/java/io/Closeable.html) |
| [WorkManagerSyncWorker](-work-manager-sync-worker/index.md) | `class WorkManagerSyncWorker : CoroutineWorker` |

### Extensions for External Classes

| Name | Summary |
|---|---|
| [kotlin.collections.List](kotlin.collections.-list/index.md) |  |

### Functions

| Name | Summary |
|---|---|
| [getLastSynced](get-last-synced.md) | `fun getLastSynced(context: <ERROR CLASS>): `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |
