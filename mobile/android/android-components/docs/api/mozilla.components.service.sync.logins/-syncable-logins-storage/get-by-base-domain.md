[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [SyncableLoginsStorage](index.md) / [getByBaseDomain](./get-by-base-domain.md)

# getByBaseDomain

`suspend fun getByBaseDomain(origin: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../../mozilla.components.concept.storage/-login/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/SyncableLoginsStorage.kt#L252)

Overrides [LoginsStorage.getByBaseDomain](../../mozilla.components.concept.storage/-logins-storage/get-by-base-domain.md)

### Exceptions

`LoginsStorageException` - On unexpected errors (IO failure, rust panics, etc)