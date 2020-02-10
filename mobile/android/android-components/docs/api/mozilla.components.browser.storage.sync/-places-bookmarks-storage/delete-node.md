[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesBookmarksStorage](index.md) / [deleteNode](./delete-node.md)

# deleteNode

`open suspend fun deleteNode(guid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesBookmarksStorage.kt#L148)

Overrides [BookmarksStorage.deleteNode](../../mozilla.components.concept.storage/-bookmarks-storage/delete-node.md)

Deletes a bookmark node and all of its children, if any.

Sync behavior: will remove bookmark from remote devices.

**Return**
Whether the bookmark existed or not.

