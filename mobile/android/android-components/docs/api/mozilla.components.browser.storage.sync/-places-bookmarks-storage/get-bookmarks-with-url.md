[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesBookmarksStorage](index.md) / [getBookmarksWithUrl](./get-bookmarks-with-url.md)

# getBookmarksWithUrl

`open suspend fun getBookmarksWithUrl(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`BookmarkNode`](../../mozilla.components.concept.storage/-bookmark-node/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesBookmarksStorage.kt#L65)

Overrides [BookmarksStorage.getBookmarksWithUrl](../../mozilla.components.concept.storage/-bookmarks-storage/get-bookmarks-with-url.md)

Produces a list of all bookmarks with the given URL.

### Parameters

`url` - The URL string.

**Return**
The list of bookmarks that match the URL

