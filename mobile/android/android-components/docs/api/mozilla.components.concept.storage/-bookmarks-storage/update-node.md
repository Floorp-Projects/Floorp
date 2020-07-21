[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [BookmarksStorage](index.md) / [updateNode](./update-node.md)

# updateNode

`abstract suspend fun updateNode(guid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, info: `[`BookmarkInfo`](../-bookmark-info/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/BookmarksStorage.kt#L90)

Edits the properties of an existing bookmark item and/or moves an existing one underneath a new parent guid.

Sync behavior: will alter bookmark item on remote devices.

### Parameters

`guid` - The guid of the item to update.

`info` - The info to change in the bookmark.