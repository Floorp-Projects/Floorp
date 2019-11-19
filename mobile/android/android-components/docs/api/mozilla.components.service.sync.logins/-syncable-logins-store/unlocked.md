[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [SyncableLoginsStore](index.md) / [unlocked](./unlocked.md)

# unlocked

`suspend fun <T> unlocked(encryptionKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, block: suspend (store: `[`LockableStore`](../../mozilla.components.concept.sync/-lockable-store/index.md)`) -> `[`T`](unlocked.md#T)`): `[`T`](unlocked.md#T) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L403)

Overrides [LockableStore.unlocked](../../mozilla.components.concept.sync/-lockable-store/unlocked.md)

Executes a [block](../../mozilla.components.concept.sync/-lockable-store/unlocked.md#mozilla.components.concept.sync.LockableStore$unlocked(kotlin.String, kotlin.SuspendFunction1((mozilla.components.concept.sync.LockableStore, mozilla.components.concept.sync.LockableStore.unlocked.T)))/block) while keeping the store in an unlocked state. Store is locked once [block](../../mozilla.components.concept.sync/-lockable-store/unlocked.md#mozilla.components.concept.sync.LockableStore$unlocked(kotlin.String, kotlin.SuspendFunction1((mozilla.components.concept.sync.LockableStore, mozilla.components.concept.sync.LockableStore.unlocked.T)))/block) is finished.

### Parameters

`encryptionKey` - Plaintext encryption key used by the underlying storage implementation (e.g. sqlcipher)
to key the store.

`block` - A lambda to execute while the store is unlocked.