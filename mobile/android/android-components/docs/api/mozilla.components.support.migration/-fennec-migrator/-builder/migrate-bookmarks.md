[android-components](../../../index.md) / [mozilla.components.support.migration](../../index.md) / [FennecMigrator](../index.md) / [Builder](index.md) / [migrateBookmarks](./migrate-bookmarks.md)

# migrateBookmarks

`fun migrateBookmarks(storage: `[`Lazy`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-lazy/index.html)`<`[`PlacesBookmarksStorage`](../../../mozilla.components.browser.storage.sync/-places-bookmarks-storage/index.md)`>, pinnedSitesStorage: `[`PinnedSiteStorage`](../../../mozilla.components.feature.top.sites/-pinned-site-storage/index.md)`? = null, version: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = Migration.Bookmarks.currentVersion): `[`Builder`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecMigrator.kt#L299)

Enable bookmarks migration. Must be called after [migrateHistory](migrate-history.md).
Optionally, enable top sites migration, if [pinnedSitesStorage](migrate-bookmarks.md#mozilla.components.support.migration.FennecMigrator.Builder$migrateBookmarks(kotlin.Lazy((mozilla.components.browser.storage.sync.PlacesBookmarksStorage)), mozilla.components.feature.top.sites.PinnedSiteStorage, kotlin.Int)/pinnedSitesStorage) is specified.
In Fennec, pinned sites are stored as special type of a bookmark, hence this coupling.

### Parameters

`storage` - An instance of [PlacesBookmarksStorage](../../../mozilla.components.browser.storage.sync/-places-bookmarks-storage/index.md), used for storing data.

`pinnedSitesStorage` - An instance of [PinnedSiteStorage](../../../mozilla.components.feature.top.sites/-pinned-site-storage/index.md), used for storing pinned sites.

`version` - Version of the migration; defaults to the current version.