[android-components](../../index.md) / [mozilla.components.feature.sync](../index.md) / [GlobalSyncableStoreProvider](./index.md)

# GlobalSyncableStoreProvider

`object GlobalSyncableStoreProvider` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/sync/src/main/java/mozilla/components/feature/sync/BackgroundSyncManager.kt#L25)

A singleton registry of [SyncableStore](../../mozilla.components.concept.sync/-syncable-store/index.md) objects. [WorkManagerSyncDispatcher](../-work-manager-sync-dispatcher/index.md) will use this to
access configured [SyncableStore](../../mozilla.components.concept.sync/-syncable-store/index.md) instances.

This pattern provides a safe way for library-defined background workers to access globally
available instances of stores within an application.

### Functions

| Name | Summary |
|---|---|
| [configureStore](configure-store.md) | `fun configureStore(storePair: `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`SyncableStore`](../../mozilla.components.concept.sync/-syncable-store/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [getStore](get-store.md) | `fun getStore(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`SyncableStore`](../../mozilla.components.concept.sync/-syncable-store/index.md)`?` |
