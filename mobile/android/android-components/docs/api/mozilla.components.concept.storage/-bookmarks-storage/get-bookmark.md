[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [BookmarksStorage](index.md) / [getBookmark](./get-bookmark.md)

# getBookmark

`abstract suspend fun getBookmark(guid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`BookmarkNode`](../-bookmark-node/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/BookmarksStorage.kt#L27)

Obtains the details of a bookmark without children, if one exists with that guid. Otherwise, null.

### Parameters

`guid` - The bookmark guid to obtain.

**Return**
The bookmark node or null if it does not exist.

