[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [AsyncLoginsStorageAdapter](index.md) / [getByBaseDomain](./get-by-base-domain.md)

# getByBaseDomain

`open fun getByBaseDomain(hostname: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`ServerPassword`](../-server-password.md)`>>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L421)

Overrides [AsyncLoginsStorage.getByBaseDomain](../-async-logins-storage/get-by-base-domain.md)

Fetch the list of passwords for some hostname from the underlying storage layer.

**RejectsWith**
[LoginsStorageException](../-logins-storage-exception.md) On unexpected errors (IO failure, rust panics, etc)

