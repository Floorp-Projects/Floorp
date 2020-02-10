[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesBookmarksStorage](index.md) / [readPinnedSitesFromFennec](./read-pinned-sites-from-fennec.md)

# readPinnedSitesFromFennec

`fun readPinnedSitesFromFennec(dbPath: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`BookmarkNode`](../../mozilla.components.concept.storage/-bookmark-node/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesBookmarksStorage.kt#L184)

Read pinned sites from Fennec's browser.db file.

### Parameters

`dbPath` - Absolute path to Fennec's browser.db file.

**Return**
A list of [BookmarkNode](../../mozilla.components.concept.storage/-bookmark-node/index.md) which represent pinned sites.

