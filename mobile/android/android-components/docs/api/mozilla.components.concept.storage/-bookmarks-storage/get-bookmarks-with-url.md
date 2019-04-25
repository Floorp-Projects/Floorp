[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [BookmarksStorage](index.md) / [getBookmarksWithUrl](./get-bookmarks-with-url.md)

# getBookmarksWithUrl

`abstract suspend fun getBookmarksWithUrl(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`BookmarkNode`](../-bookmark-node/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/BookmarksStorage.kt#L35)

Produces a list of all bookmarks with the given URL.

### Parameters

`url` - The URL string.

**Return**
The list of bookmarks that match the URL

