[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesBookmarksStorage](index.md) / [getBookmark](./get-bookmark.md)

# getBookmark

`open suspend fun getBookmark(guid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`BookmarkNode`](../../mozilla.components.concept.storage/-bookmark-node/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesBookmarksStorage.kt#L48)

Overrides [BookmarksStorage.getBookmark](../../mozilla.components.concept.storage/-bookmarks-storage/get-bookmark.md)

Obtains the details of a bookmark without children, if one exists with that guid. Otherwise, null.

### Parameters

`guid` - The bookmark guid to obtain.

**Return**
The bookmark node or null if it does not exist.

