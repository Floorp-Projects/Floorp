[android-components](../../index.md) / [mozilla.components.feature.sync](../index.md) / [GeneralSyncManager](index.md) / [addStore](./add-store.md)

# addStore

`open fun addStore(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/sync/src/main/java/mozilla/components/feature/sync/BackgroundSyncManager.kt#L114)

Overrides [SyncManager.addStore](../../mozilla.components.concept.sync/-sync-manager/add-store.md)

Add a store, by [name](../../mozilla.components.concept.sync/-sync-manager/add-store.md#mozilla.components.concept.sync.SyncManager$addStore(kotlin.String)/name), to a set of synchronization stores.
Implementation is expected to be able to either access or instantiate concrete
[SyncableStore](../../mozilla.components.concept.sync/-syncable-store/index.md) instances given this name.

