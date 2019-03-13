[android-components](../../index.md) / [mozilla.components.feature.sync](../index.md) / [StorageSync](./index.md)

# StorageSync

`class StorageSync : `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`SyncStatusObserver`](../../mozilla.components.concept.sync/-sync-status-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/sync/src/main/java/mozilla/components/feature/sync/StorageSync.kt#L25)

A feature implementation which orchestrates data synchronization of a set of [SyncableStore](../../mozilla.components.concept.sync/-syncable-store/index.md)-s.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `StorageSync(syncableStores: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`SyncableStore`](../../mozilla.components.concept.sync/-syncable-store/index.md)`>, syncScope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>A feature implementation which orchestrates data synchronization of a set of [SyncableStore](../../mozilla.components.concept.sync/-syncable-store/index.md)-s. |

### Functions

| Name | Summary |
|---|---|
| [sync](sync.md) | `suspend fun sync(account: `[`OAuthAccount`](../../mozilla.components.concept.sync/-o-auth-account/index.md)`): `[`SyncResult`](../../mozilla.components.concept.sync/-sync-result.md)<br>Performs a sync of configured [SyncableStore](../../mozilla.components.concept.sync/-syncable-store/index.md) history instance. This method guarantees that only one sync may be running at any given time. |
