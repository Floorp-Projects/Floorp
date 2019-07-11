[android-components](../../index.md) / [mozilla.components.service.fxa.sync](../index.md) / [StorageSync](./index.md)

# StorageSync

`class StorageSync : `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`SyncStatusObserver`](../-sync-status-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/sync/StorageSync.kt#L25)

Orchestrates data synchronization of a set of [SyncableStore](../../mozilla.components.concept.sync/-syncable-store/index.md)-s.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `StorageSync(syncableStores: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`SyncableStore`](../../mozilla.components.concept.sync/-syncable-store/index.md)`>)`<br>Orchestrates data synchronization of a set of [SyncableStore](../../mozilla.components.concept.sync/-syncable-store/index.md)-s. |

### Functions

| Name | Summary |
|---|---|
| [sync](sync.md) | `suspend fun sync(authInfo: `[`SyncAuthInfo`](../../mozilla.components.concept.sync/-sync-auth-info/index.md)`): `[`SyncResult`](../../mozilla.components.concept.sync/-sync-result.md)<br>Performs a sync of configured [SyncableStore](../../mozilla.components.concept.sync/-syncable-store/index.md) history instance. This method guarantees that only one sync may be running at any given time. |
