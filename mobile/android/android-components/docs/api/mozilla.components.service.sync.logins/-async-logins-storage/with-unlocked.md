[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [AsyncLoginsStorage](index.md) / [withUnlocked](./with-unlocked.md)

# withUnlocked

`open suspend fun <T> withUnlocked(key: () -> Deferred<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, block: suspend (`[`AsyncLoginsStorage`](index.md)`) -> `[`T`](with-unlocked.md#T)`): `[`T`](with-unlocked.md#T) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L314)

Run some [block](with-unlocked.md#mozilla.components.service.sync.logins.AsyncLoginsStorage$withUnlocked(kotlin.Function0((kotlinx.coroutines.Deferred((kotlin.String)))), kotlin.SuspendFunction1((mozilla.components.service.sync.logins.AsyncLoginsStorage, mozilla.components.service.sync.logins.AsyncLoginsStorage.withUnlocked.T)))/block) which operates over an unlocked instance of [AsyncLoginsStorage](index.md).
Database is locked once [block](with-unlocked.md#mozilla.components.service.sync.logins.AsyncLoginsStorage$withUnlocked(kotlin.Function0((kotlinx.coroutines.Deferred((kotlin.String)))), kotlin.SuspendFunction1((mozilla.components.service.sync.logins.AsyncLoginsStorage, mozilla.components.service.sync.logins.AsyncLoginsStorage.withUnlocked.T)))/block) is done.

### Exceptions

`InvalidKeyException` - if the provided [key](with-unlocked.md#mozilla.components.service.sync.logins.AsyncLoginsStorage$withUnlocked(kotlin.Function0((kotlinx.coroutines.Deferred((kotlin.String)))), kotlin.SuspendFunction1((mozilla.components.service.sync.logins.AsyncLoginsStorage, mozilla.components.service.sync.logins.AsyncLoginsStorage.withUnlocked.T)))/key) isn't valid.