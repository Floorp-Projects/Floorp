[android-components](../../index.md) / [mozilla.components.service.fxa.sync](../index.md) / [GlobalSyncableStoreProvider](index.md) / [configureStore](./configure-store.md)

# configureStore

`fun configureStore(storePair: `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`SyncEngine`](../../mozilla.components.service.fxa/-sync-engine/index.md)`, `[`Lazy`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-lazy/index.html)`<`[`SyncableStore`](../../mozilla.components.concept.sync/-syncable-store/index.md)`>>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/sync/SyncManager.kt#L85)

Configure an instance of [SyncableStore](../../mozilla.components.concept.sync/-syncable-store/index.md) for a [SyncEngine](../../mozilla.components.service.fxa/-sync-engine/index.md) enum.

### Parameters

`storePair` - A pair associating [SyncableStore](../../mozilla.components.concept.sync/-syncable-store/index.md) with a [SyncEngine](../../mozilla.components.service.fxa/-sync-engine/index.md).