[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesBookmarksStorage](index.md) / [addFolder](./add-folder.md)

# addFolder

`open suspend fun addFolder(parentGuid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesBookmarksStorage.kt#L106)

Overrides [BookmarksStorage.addFolder](../../mozilla.components.concept.storage/-bookmarks-storage/add-folder.md)

Adds a new bookmark folder to a given node.

Sync behavior: will add new separator to remote devices.

### Parameters

`parentGuid` - The parent guid of the new node.

`title` - The title of the bookmark folder to add.

`position` - The optional position to add the new node or null to append.

**Return**
The guid of the newly inserted bookmark item.

