[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesBookmarksStorage](index.md) / [updateNode](./update-node.md)

# updateNode

`open suspend fun updateNode(guid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, info: `[`BookmarkInfo`](../../mozilla.components.concept.storage/-bookmark-info/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesBookmarksStorage.kt#L140)

Overrides [BookmarksStorage.updateNode](../../mozilla.components.concept.storage/-bookmarks-storage/update-node.md)

Edits the properties of an existing bookmark item and/or moves an existing one underneath a new parent guid.

Sync behavior: will alter bookmark item on remote devices.

### Parameters

`guid` - The guid of the item to update.

`info` - The info to change in the bookmark.