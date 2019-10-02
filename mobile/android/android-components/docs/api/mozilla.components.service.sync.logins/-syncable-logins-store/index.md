[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [SyncableLoginsStore](./index.md)

# SyncableLoginsStore

`data class SyncableLoginsStore : `[`SyncableStore`](../../mozilla.components.concept.sync/-syncable-store/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L388)

Wraps [AsyncLoginsStorage](../-async-logins-storage/index.md) instance along with a lazy encryption key.

This helper class lives here and not alongside [AsyncLoginsStorage](../-async-logins-storage/index.md) because we don't want to
force a `service-sync-logins` dependency (which has a heavy native library dependency) on
consumers of [FirefoxSyncFeature](#).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SyncableLoginsStore(store: `[`AsyncLoginsStorage`](../-async-logins-storage/index.md)`, key: () -> Deferred<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>)`<br>Wraps [AsyncLoginsStorage](../-async-logins-storage/index.md) instance along with a lazy encryption key. |

### Properties

| Name | Summary |
|---|---|
| [key](key.md) | `val key: () -> Deferred<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` |
| [store](store.md) | `val store: `[`AsyncLoginsStorage`](../-async-logins-storage/index.md) |

### Functions

| Name | Summary |
|---|---|
| [getHandle](get-handle.md) | `fun getHandle(): `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>This should be removed. See: https://github.com/mozilla/application-services/issues/1877 |
| [sync](sync.md) | `suspend fun sync(authInfo: `[`SyncAuthInfo`](../../mozilla.components.concept.sync/-sync-auth-info/index.md)`): `[`SyncStatus`](../../mozilla.components.concept.sync/-sync-status/index.md)<br>Performs a sync. |
| [withUnlocked](with-unlocked.md) | `suspend fun <T> withUnlocked(block: suspend (`[`AsyncLoginsStorage`](../-async-logins-storage/index.md)`) -> `[`T`](with-unlocked.md#T)`): `[`T`](with-unlocked.md#T)<br>Run some [block](with-unlocked.md#mozilla.components.service.sync.logins.SyncableLoginsStore$withUnlocked(kotlin.SuspendFunction1((mozilla.components.service.sync.logins.AsyncLoginsStorage, mozilla.components.service.sync.logins.SyncableLoginsStore.withUnlocked.T)))/block) which operates over an unlocked instance of [AsyncLoginsStorage](../-async-logins-storage/index.md). Database is locked once [block](with-unlocked.md#mozilla.components.service.sync.logins.SyncableLoginsStore$withUnlocked(kotlin.SuspendFunction1((mozilla.components.service.sync.logins.AsyncLoginsStorage, mozilla.components.service.sync.logins.SyncableLoginsStore.withUnlocked.T)))/block) is done. |
