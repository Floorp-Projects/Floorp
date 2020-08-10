[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [SyncableLoginsStorage](index.md) / [getPotentialDupesIgnoringUsername](./get-potential-dupes-ignoring-username.md)

# getPotentialDupesIgnoringUsername

`suspend fun getPotentialDupesIgnoringUsername(login: `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../../mozilla.components.concept.storage/-login/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/SyncableLoginsStorage.kt#L260)

Overrides [LoginsStorage.getPotentialDupesIgnoringUsername](../../mozilla.components.concept.storage/-logins-storage/get-potential-dupes-ignoring-username.md)

### Exceptions

`LoginsStorageException` - On unexpected errors (IO failure, rust panics, etc)