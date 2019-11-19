[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [LockableStore](index.md) / [unlocked](./unlocked.md)

# unlocked

`abstract suspend fun <T> unlocked(encryptionKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, block: suspend (store: `[`LockableStore`](index.md)`) -> `[`T`](unlocked.md#T)`): `[`T`](unlocked.md#T) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Sync.kt#L56)

Executes a [block](unlocked.md#mozilla.components.concept.sync.LockableStore$unlocked(kotlin.String, kotlin.SuspendFunction1((mozilla.components.concept.sync.LockableStore, mozilla.components.concept.sync.LockableStore.unlocked.T)))/block) while keeping the store in an unlocked state. Store is locked once [block](unlocked.md#mozilla.components.concept.sync.LockableStore$unlocked(kotlin.String, kotlin.SuspendFunction1((mozilla.components.concept.sync.LockableStore, mozilla.components.concept.sync.LockableStore.unlocked.T)))/block) is finished.

### Parameters

`encryptionKey` - Plaintext encryption key used by the underlying storage implementation (e.g. sqlcipher)
to key the store.

`block` - A lambda to execute while the store is unlocked.