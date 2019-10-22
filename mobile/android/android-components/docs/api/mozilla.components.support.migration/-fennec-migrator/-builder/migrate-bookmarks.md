[android-components](../../../index.md) / [mozilla.components.support.migration](../../index.md) / [FennecMigrator](../index.md) / [Builder](index.md) / [migrateBookmarks](./migrate-bookmarks.md)

# migrateBookmarks

`fun migrateBookmarks(storage: `[`PlacesBookmarksStorage`](../../../mozilla.components.browser.storage.sync/-places-bookmarks-storage/index.md)`, version: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = Migration.Bookmarks.currentVersion): `[`Builder`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecMigrator.kt#L111)

Enable bookmarks migration. Must be called after [migrateHistory](migrate-history.md).

### Parameters

`storage` - An instance of [PlacesBookmarksStorage](../../../mozilla.components.browser.storage.sync/-places-bookmarks-storage/index.md), used for storing data.

`version` - Version of the migration; defaults to the current version.