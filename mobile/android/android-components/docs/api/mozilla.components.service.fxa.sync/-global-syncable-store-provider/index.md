[android-components](../../index.md) / [mozilla.components.service.fxa.sync](../index.md) / [GlobalSyncableStoreProvider](./index.md)

# GlobalSyncableStoreProvider

`object GlobalSyncableStoreProvider` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/sync/SyncManager.kt#L78)

A singleton registry of [SyncableStore](../../mozilla.components.concept.sync/-syncable-store/index.md) objects. [WorkManagerSyncDispatcher](../-work-manager-sync-dispatcher/index.md) will use this to
access configured [SyncableStore](../../mozilla.components.concept.sync/-syncable-store/index.md) instances.

This pattern provides a safe way for library-defined background workers to access globally
available instances of stores within an application.

### Functions

| Name | Summary |
|---|---|
| [configureStore](configure-store.md) | `fun configureStore(storePair: `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`SyncEngine`](../../mozilla.components.service.fxa/-sync-engine/index.md)`, `[`SyncableStore`](../../mozilla.components.concept.sync/-syncable-store/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
