[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [SyncManager](index.md) / [addStore](./add-store.md)

# addStore

`abstract fun addStore(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Sync.kt#L57)

Add a store, by [name](add-store.md#mozilla.components.concept.sync.SyncManager$addStore(kotlin.String)/name), to a set of synchronization stores.
Implementation is expected to be able to either access or instantiate concrete
[SyncableStore](../-syncable-store/index.md) instances given this name.

