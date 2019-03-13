[android-components](../index.md) / [mozilla.components.feature.sync](./index.md)

## Package mozilla.components.feature.sync

### Types

| Name | Summary |
|---|---|
| [BackgroundSyncManager](-background-sync-manager/index.md) | `class BackgroundSyncManager : `[`GeneralSyncManager`](-general-sync-manager/index.md)<br>A SyncManager implementation which uses WorkManager APIs to schedule sync tasks. |
| [GeneralSyncManager](-general-sync-manager/index.md) | `abstract class GeneralSyncManager : `[`SyncManager`](../mozilla.components.concept.sync/-sync-manager/index.md)`, `[`Observable`](../mozilla.components.support.base.observer/-observable/index.md)`<`[`SyncStatusObserver`](../mozilla.components.concept.sync/-sync-status-observer/index.md)`>, `[`SyncStatusObserver`](../mozilla.components.concept.sync/-sync-status-observer/index.md)<br>A base SyncManager implementation which manages a dispatcher, handles authentication and requests synchronization in the following manner: On authentication AND on store set changes (add or remove)... |
| [GlobalSyncableStoreProvider](-global-syncable-store-provider/index.md) | `object GlobalSyncableStoreProvider`<br>A singleton registry of [SyncableStore](../mozilla.components.concept.sync/-syncable-store/index.md) objects. [WorkManagerSyncDispatcher](-work-manager-sync-dispatcher/index.md) will use this to access configured [SyncableStore](../mozilla.components.concept.sync/-syncable-store/index.md) instances. |
| [StorageSync](-storage-sync/index.md) | `class StorageSync : `[`Observable`](../mozilla.components.support.base.observer/-observable/index.md)`<`[`SyncStatusObserver`](../mozilla.components.concept.sync/-sync-status-observer/index.md)`>`<br>A feature implementation which orchestrates data synchronization of a set of [SyncableStore](../mozilla.components.concept.sync/-syncable-store/index.md)-s. |
| [SyncDispatcher](-sync-dispatcher/index.md) | `interface SyncDispatcher : `[`Closeable`](https://developer.android.com/reference/java/io/Closeable.html)`, `[`Observable`](../mozilla.components.support.base.observer/-observable/index.md)`<`[`SyncStatusObserver`](../mozilla.components.concept.sync/-sync-status-observer/index.md)`>`<br>Internal interface to enable testing SyncManager implementations independently from SyncDispatcher. |
| [WorkManagerSyncDispatcher](-work-manager-sync-dispatcher/index.md) | `class WorkManagerSyncDispatcher : `[`SyncDispatcher`](-sync-dispatcher/index.md)`, `[`Observable`](../mozilla.components.support.base.observer/-observable/index.md)`<`[`SyncStatusObserver`](../mozilla.components.concept.sync/-sync-status-observer/index.md)`>, `[`Closeable`](https://developer.android.com/reference/java/io/Closeable.html) |
| [WorkManagerSyncWorker](-work-manager-sync-worker/index.md) | `class WorkManagerSyncWorker : CoroutineWorker` |
| [WorkersLiveDataObserver](-workers-live-data-observer/index.md) | `object WorkersLiveDataObserver`<br>A singleton wrapper around the the LiveData "forever" observer - i.e. an observer not bound to a lifecycle owner. This observer is always active. We will have different dispatcher instances throughout the lifetime of the app, but always a single LiveData instance. |

### Functions

| Name | Summary |
|---|---|
| [getLastSynced](get-last-synced.md) | `fun getLastSynced(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`): `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |
| [setLastSynced](set-last-synced.md) | `fun setLastSynced(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, ts: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
