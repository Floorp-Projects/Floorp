[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [BookmarksStorage](index.md) / [getTree](./get-tree.md)

# getTree

`abstract suspend fun getTree(guid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, recursive: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`BookmarkNode`](../-bookmark-node/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/BookmarksStorage.kt#L19)

Produces a bookmarks tree for the given guid string.

### Parameters

`guid` - The bookmark guid to obtain.

`recursive` - Whether to recurse and obtain all levels of children.

**Return**
The populated root starting from the guid.

