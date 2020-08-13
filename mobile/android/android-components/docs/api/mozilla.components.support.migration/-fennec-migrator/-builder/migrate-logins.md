[android-components](../../../index.md) / [mozilla.components.support.migration](../../index.md) / [FennecMigrator](../index.md) / [Builder](index.md) / [migrateLogins](./migrate-logins.md)

# migrateLogins

`fun migrateLogins(storage: `[`Lazy`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-lazy/index.html)`<`[`SyncableLoginsStorage`](../../../mozilla.components.service.sync.logins/-syncable-logins-storage/index.md)`>, version: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = Migration.Logins.currentVersion): `[`Builder`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecMigrator.kt#L328)

Enable logins migration.

### Parameters

`storage` - An instance of [AsyncLoginsStorage](#), used for storing data.