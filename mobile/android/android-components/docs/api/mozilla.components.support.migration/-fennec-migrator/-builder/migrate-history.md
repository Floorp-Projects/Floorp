[android-components](../../../index.md) / [mozilla.components.support.migration](../../index.md) / [FennecMigrator](../index.md) / [Builder](index.md) / [migrateHistory](./migrate-history.md)

# migrateHistory

`fun migrateHistory(storage: `[`Lazy`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-lazy/index.html)`<`[`PlacesHistoryStorage`](../../../mozilla.components.browser.storage.sync/-places-history-storage/index.md)`>, version: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = Migration.History.currentVersion): `[`Builder`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecMigrator.kt#L277)

Enable history migration.

### Parameters

`storage` - An instance of [PlacesHistoryStorage](../../../mozilla.components.browser.storage.sync/-places-history-storage/index.md), used for storing data.

`version` - Version of the migration; defaults to the current version.