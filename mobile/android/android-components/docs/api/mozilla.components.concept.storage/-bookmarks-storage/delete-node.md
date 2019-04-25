[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [BookmarksStorage](index.md) / [deleteNode](./delete-node.md)

# deleteNode

`abstract suspend fun deleteNode(guid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/BookmarksStorage.kt#L99)

Deletes a bookmark node and all of its children, if any.

Sync behavior: will remove bookmark from remote devices.

**Return**
Whether the bookmark existed or not.

