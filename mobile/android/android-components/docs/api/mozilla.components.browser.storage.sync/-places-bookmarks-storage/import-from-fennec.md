[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesBookmarksStorage](index.md) / [importFromFennec](./import-from-fennec.md)

# importFromFennec

`fun importFromFennec(dbPath: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesBookmarksStorage.kt#L174)

Import bookmarks data from Fennec's browser.db file.
Before running this, first run [PlacesHistoryStorage.importFromFennec](../-places-history-storage/import-from-fennec.md) to import history and visits data.

### Parameters

`dbPath` - Absolute path to Fennec's browser.db file.

**Return**
Migration metrics wrapped in a JSON object. See libplaces for schema details.

