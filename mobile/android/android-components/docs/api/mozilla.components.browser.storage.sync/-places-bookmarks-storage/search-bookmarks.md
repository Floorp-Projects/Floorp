[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesBookmarksStorage](index.md) / [searchBookmarks](./search-bookmarks.md)

# searchBookmarks

`open suspend fun searchBookmarks(query: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, limit: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`BookmarkNode`](../../mozilla.components.concept.storage/-bookmark-node/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesBookmarksStorage.kt#L73)

Overrides [BookmarksStorage.searchBookmarks](../../mozilla.components.concept.storage/-bookmarks-storage/search-bookmarks.md)

Searches bookmarks with a query string.

### Parameters

`query` - The query string to search.

`limit` - The maximum number of entries to return.

**Return**
The list of matching bookmark nodes up to the limit number of items.

