[android-components](../../../index.md) / [mozilla.components.support.migration](../../index.md) / [FennecMigrator](../index.md) / [Builder](index.md) / [migrateSearchEngine](./migrate-search-engine.md)

# migrateSearchEngine

`fun migrateSearchEngine(searchEngineManager: `[`SearchEngineManager`](../../../mozilla.components.browser.search/-search-engine-manager/index.md)`, version: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = Migration.SearchEngine.currentVersion): `[`Builder`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecMigrator.kt#L369)

Enable default search engine migration.

### Parameters

`searchEngineManager` - An instance of [SearchEngineManager](../../../mozilla.components.browser.search/-search-engine-manager/index.md) used for restoring the
migrated default search engine.

`version` - Version of the migration; defaults to the current version.