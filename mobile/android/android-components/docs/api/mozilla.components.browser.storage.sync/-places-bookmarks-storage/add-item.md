[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesBookmarksStorage](index.md) / [addItem](./add-item.md)

# addItem

`open suspend fun addItem(parentGuid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesBookmarksStorage.kt#L90)

Overrides [BookmarksStorage.addItem](../../mozilla.components.concept.storage/-bookmarks-storage/add-item.md)

Adds a new bookmark item to a given node.

Sync behavior: will add new bookmark item to remote devices.

### Parameters

`parentGuid` - The parent guid of the new node.

`url` - The URL of the bookmark item to add.

`title` - The title of the bookmark item to add.

`position` - The optional position to add the new node or null to append.

**Return**
The guid of the newly inserted bookmark item.

