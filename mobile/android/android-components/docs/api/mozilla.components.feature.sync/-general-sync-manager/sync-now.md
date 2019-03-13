[android-components](../../index.md) / [mozilla.components.feature.sync](../index.md) / [GeneralSyncManager](index.md) / [syncNow](./sync-now.md)

# syncNow

`open fun syncNow(startup: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/sync/src/main/java/mozilla/components/feature/sync/BackgroundSyncManager.kt#L150)

Overrides [SyncManager.syncNow](../../mozilla.components.concept.sync/-sync-manager/sync-now.md)

Request an immediate synchronization of all configured stores.

### Parameters

`startup` - Boolean flag indicating if sync is being requested in a startup situation.