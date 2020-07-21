[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [SyncableLoginsStorage](index.md) / [importLoginsAsync](./import-logins-async.md)

# importLoginsAsync

`suspend fun importLoginsAsync(logins: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../../mozilla.components.concept.storage/-login/index.md)`>): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/SyncableLoginsStorage.kt#L234)

Overrides [LoginsStorage.importLoginsAsync](../../mozilla.components.concept.storage/-logins-storage/import-logins-async.md)

### Exceptions

`LoginsStorageException` - If DB isn't empty during an import; also, on unexpected errors
(IO failure, rust panics, etc).