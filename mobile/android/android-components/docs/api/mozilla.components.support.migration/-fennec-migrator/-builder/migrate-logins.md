[android-components](../../../index.md) / [mozilla.components.support.migration](../../index.md) / [FennecMigrator](../index.md) / [Builder](index.md) / [migrateLogins](./migrate-logins.md)

# migrateLogins

`fun migrateLogins(storage: `[`AsyncLoginsStorage`](../../../mozilla.components.service.sync.logins/-async-logins-storage/index.md)`, storageKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, version: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = Migration.Logins.currentVersion): `[`Builder`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecMigrator.kt#L239)

Enable logins migration.

### Parameters

`storage` - An instance of [AsyncLoginsStorage](../../../mozilla.components.service.sync.logins/-async-logins-storage/index.md), used for storing data.