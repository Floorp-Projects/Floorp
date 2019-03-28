[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesBookmarksStorage](./index.md)

# PlacesBookmarksStorage

`open class PlacesBookmarksStorage : `[`PlacesStorage`](../-places-storage/index.md)`, `[`BookmarksStorage`](../../mozilla.components.concept.storage/-bookmarks-storage/index.md)`, `[`SyncableStore`](../../mozilla.components.concept.sync/-syncable-store/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesBookmarksStorage.kt#L23)

Implementation of the [BookmarksStorage](../../mozilla.components.concept.storage/-bookmarks-storage/index.md) which is backed by a Rust Places lib via [PlacesApi](#).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `PlacesBookmarksStorage(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`)`<br>Implementation of the [BookmarksStorage](../../mozilla.components.concept.storage/-bookmarks-storage/index.md) which is backed by a Rust Places lib via [PlacesApi](#). |

### Functions

| Name | Summary |
|---|---|
| [addFolder](add-folder.md) | `open suspend fun addFolder(parentGuid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Adds a new bookmark folder to a given node. |
| [addItem](add-item.md) | `open suspend fun addItem(parentGuid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Adds a new bookmark item to a given node. |
| [addSeparator](add-separator.md) | `open suspend fun addSeparator(parentGuid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Adds a new bookmark separator to a given node. |
| [deleteNode](delete-node.md) | `open suspend fun deleteNode(guid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Deletes a bookmark node and all of its children, if any. |
| [getTree](get-tree.md) | `open suspend fun getTree(guid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, recursive: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`BookmarkNode`](../../mozilla.components.concept.storage/-bookmark-node/index.md)`?`<br>Produces a bookmarks tree for the given guid string. |
| [searchBookmarks](search-bookmarks.md) | `open suspend fun searchBookmarks(query: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, limit: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`BookmarkNode`](../../mozilla.components.concept.storage/-bookmark-node/index.md)`>`<br>Searches bookmarks with a query string. |
| [updateNode](update-node.md) | `open suspend fun updateNode(guid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, info: `[`BookmarkInfo`](../../mozilla.components.concept.storage/-bookmark-info/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Edits the properties of an existing bookmark item and/or moves an existing one underneath a new parent guid. |

### Inherited Functions

| Name | Summary |
|---|---|
| [cleanup](../-places-storage/cleanup.md) | `open fun cleanup(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Cleans up background work and database connections |
| [runMaintenance](../-places-storage/run-maintenance.md) | `open suspend fun runMaintenance(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Internal database maintenance tasks. Ideally this should be called once a day. |
| [sync](../-places-storage/sync.md) | `open suspend fun sync(authInfo: `[`AuthInfo`](../../mozilla.components.concept.sync/-auth-info/index.md)`): `[`SyncStatus`](../../mozilla.components.concept.sync/-sync-status/index.md)<br>Runs sync() method on the places Connection |
