[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesHistoryStorage](index.md) / [importFromFennec](./import-from-fennec.md)

# importFromFennec

`fun importFromFennec(dbPath: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesHistoryStorage.kt#L212)

Import history and visits data from Fennec's browser.db file.

### Parameters

`dbPath` - Absolute path to Fennec's browser.db file.

**Return**
Migration metrics wrapped in a JSON object. See libplaces for schema details.

