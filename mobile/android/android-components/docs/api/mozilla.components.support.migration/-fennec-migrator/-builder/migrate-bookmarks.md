[android-components](../../../index.md) / [mozilla.components.support.migration](../../index.md) / [FennecMigrator](../index.md) / [Builder](index.md) / [migrateBookmarks](./migrate-bookmarks.md)

# migrateBookmarks

`fun migrateBookmarks(storage: `[`Lazy`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-lazy/index.html)`<`[`PlacesBookmarksStorage`](../../../mozilla.components.browser.storage.sync/-places-bookmarks-storage/index.md)`>, topSiteStorage: `[`TopSiteStorage`](../../../mozilla.components.feature.top.sites/-top-site-storage/index.md)`? = null, version: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = Migration.Bookmarks.currentVersion): `[`Builder`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecMigrator.kt#L286)

Enable bookmarks migration. Must be called after [migrateHistory](migrate-history.md).
Optionally, enable top sites migration, if [topSiteStorage](migrate-bookmarks.md#mozilla.components.support.migration.FennecMigrator.Builder$migrateBookmarks(kotlin.Lazy((mozilla.components.browser.storage.sync.PlacesBookmarksStorage)), mozilla.components.feature.top.sites.TopSiteStorage, kotlin.Int)/topSiteStorage) is specified.
In Fennec, pinned sites are stored as special type of a bookmark, hence this coupling.

### Parameters

`storage` - An instance of [PlacesBookmarksStorage](../../../mozilla.components.browser.storage.sync/-places-bookmarks-storage/index.md), used for storing data.

`topSiteStorage` - An instance of [TopSiteStorage](../../../mozilla.components.feature.top.sites/-top-site-storage/index.md), used for storing pinned sites.

`version` - Version of the migration; defaults to the current version.