[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [LockableStore](./index.md)

# LockableStore

`interface LockableStore : `[`SyncableStore`](../-syncable-store/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Sync.kt#L48)

An extension of [SyncableStore](../-syncable-store/index.md) that can be locked/unlocked using an encryption key.

### Functions

| Name | Summary |
|---|---|
| [unlocked](unlocked.md) | `abstract suspend fun <T> unlocked(encryptionKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, block: suspend (store: `[`LockableStore`](./index.md)`) -> `[`T`](unlocked.md#T)`): `[`T`](unlocked.md#T)<br>Executes a [block](unlocked.md#mozilla.components.concept.sync.LockableStore$unlocked(kotlin.String, kotlin.SuspendFunction1((mozilla.components.concept.sync.LockableStore, mozilla.components.concept.sync.LockableStore.unlocked.T)))/block) while keeping the store in an unlocked state. Store is locked once [block](unlocked.md#mozilla.components.concept.sync.LockableStore$unlocked(kotlin.String, kotlin.SuspendFunction1((mozilla.components.concept.sync.LockableStore, mozilla.components.concept.sync.LockableStore.unlocked.T)))/block) is finished. |

### Inherited Functions

| Name | Summary |
|---|---|
| [getHandle](../-syncable-store/get-handle.md) | `abstract fun getHandle(): `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>This should be removed. See: https://github.com/mozilla/application-services/issues/1877 |
| [sync](../-syncable-store/sync.md) | `abstract suspend fun sync(authInfo: `[`SyncAuthInfo`](../-sync-auth-info/index.md)`): `[`SyncStatus`](../-sync-status/index.md)<br>Performs a sync. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [SyncableLoginsStore](../../mozilla.components.service.sync.logins/-syncable-logins-store/index.md) | `data class SyncableLoginsStore : `[`LockableStore`](./index.md)<br>Wraps [AsyncLoginsStorage](../../mozilla.components.service.sync.logins/-async-logins-storage/index.md) instance along with a lazy encryption key. |
