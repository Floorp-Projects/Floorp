[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [BookmarksStorage](index.md) / [searchBookmarks](./search-bookmarks.md)

# searchBookmarks

`abstract suspend fun searchBookmarks(query: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, limit: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = defaultBookmarkSearchLimit): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`BookmarkNode`](../-bookmark-node/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/BookmarksStorage.kt#L44)

Searches bookmarks with a query string.

### Parameters

`query` - The query string to search.

`limit` - The maximum number of entries to return.

**Return**
The list of matching bookmark nodes up to the limit number of items.

