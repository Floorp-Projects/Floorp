[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [SyncableLoginsStore](index.md) / [withUnlocked](./with-unlocked.md)

# withUnlocked

`suspend fun <T> withUnlocked(block: suspend (`[`AsyncLoginsStorage`](../-async-logins-storage/index.md)`) -> `[`T`](with-unlocked.md#T)`): `[`T`](with-unlocked.md#T) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L413)

Run some [block](with-unlocked.md#mozilla.components.service.sync.logins.SyncableLoginsStore$withUnlocked(kotlin.SuspendFunction1((mozilla.components.service.sync.logins.AsyncLoginsStorage, mozilla.components.service.sync.logins.SyncableLoginsStore.withUnlocked.T)))/block) which operates over an unlocked instance of [AsyncLoginsStorage](../-async-logins-storage/index.md).
Database is locked once [block](with-unlocked.md#mozilla.components.service.sync.logins.SyncableLoginsStore$withUnlocked(kotlin.SuspendFunction1((mozilla.components.service.sync.logins.AsyncLoginsStorage, mozilla.components.service.sync.logins.SyncableLoginsStore.withUnlocked.T)))/block) is done.

### Exceptions

`InvalidKeyException` - if the provided [key](key.md) isn't valid.