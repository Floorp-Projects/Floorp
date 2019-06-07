[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesBookmarksStorage](index.md) / [getTree](./get-tree.md)

# getTree

`open suspend fun getTree(guid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, recursive: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`BookmarkNode`](../../mozilla.components.concept.storage/-bookmark-node/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesBookmarksStorage.kt#L39)

Overrides [BookmarksStorage.getTree](../../mozilla.components.concept.storage/-bookmarks-storage/get-tree.md)

Produces a bookmarks tree for the given guid string.

### Parameters

`guid` - The bookmark guid to obtain.

`recursive` - Whether to recurse and obtain all levels of children.

**Return**
The populated root starting from the guid.

