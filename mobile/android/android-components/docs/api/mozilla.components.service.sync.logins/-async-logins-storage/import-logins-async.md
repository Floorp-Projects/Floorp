[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [AsyncLoginsStorage](index.md) / [importLoginsAsync](./import-logins-async.md)

# importLoginsAsync

`abstract fun importLoginsAsync(logins: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`ServerPassword`](../-server-password.md)`>): Deferred<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L288)

Bulk-import of a list of [ServerPassword](../-server-password.md).
Storage must be empty, otherwise underlying implementation this will throw.

**Return**
number of records that could not be imported.

**RejectsWith**
[LoginsStorageException](../-logins-storage-exception.md) If DB isn't empty during an import; also, on unexpected errors
(IO failure, rust panics, etc).

