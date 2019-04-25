[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [BookmarksStorage](./index.md)

# BookmarksStorage

`interface BookmarksStorage : `[`Storage`](../-storage/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/BookmarksStorage.kt#L10)

An interface which defines read/write operations for bookmarks data.

### Functions

| Name | Summary |
|---|---|
| [addFolder](add-folder.md) | `abstract suspend fun addFolder(parentGuid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`? = null): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Adds a new bookmark folder to a given node. |
| [addItem](add-item.md) | `abstract suspend fun addItem(parentGuid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Adds a new bookmark item to a given node. |
| [addSeparator](add-separator.md) | `abstract suspend fun addSeparator(parentGuid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Adds a new bookmark separator to a given node. |
| [deleteNode](delete-node.md) | `abstract suspend fun deleteNode(guid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Deletes a bookmark node and all of its children, if any. |
| [getBookmark](get-bookmark.md) | `abstract suspend fun getBookmark(guid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`BookmarkNode`](../-bookmark-node/index.md)`?`<br>Obtains the details of a bookmark without children, if one exists with that guid. Otherwise, null. |
| [getBookmarksWithUrl](get-bookmarks-with-url.md) | `abstract suspend fun getBookmarksWithUrl(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`BookmarkNode`](../-bookmark-node/index.md)`>`<br>Produces a list of all bookmarks with the given URL. |
| [getTree](get-tree.md) | `abstract suspend fun getTree(guid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, recursive: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`BookmarkNode`](../-bookmark-node/index.md)`?`<br>Produces a bookmarks tree for the given guid string. |
| [searchBookmarks](search-bookmarks.md) | `abstract suspend fun searchBookmarks(query: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, limit: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = defaultBookmarkSearchLimit): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`BookmarkNode`](../-bookmark-node/index.md)`>`<br>Searches bookmarks with a query string. |
| [updateNode](update-node.md) | `abstract suspend fun updateNode(guid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, info: `[`BookmarkInfo`](../-bookmark-info/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Edits the properties of an existing bookmark item and/or moves an existing one underneath a new parent guid. |

### Inherited Functions

| Name | Summary |
|---|---|
| [cleanup](../-storage/cleanup.md) | `abstract fun cleanup(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Cleans up background work and database connections |
| [runMaintenance](../-storage/run-maintenance.md) | `abstract suspend fun runMaintenance(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Runs internal database maintenance tasks |

### Companion Object Properties

| Name | Summary |
|---|---|
| [defaultBookmarkSearchLimit](default-bookmark-search-limit.md) | `const val defaultBookmarkSearchLimit: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [PlacesBookmarksStorage](../../mozilla.components.browser.storage.sync/-places-bookmarks-storage/index.md) | `open class PlacesBookmarksStorage : `[`PlacesStorage`](../../mozilla.components.browser.storage.sync/-places-storage/index.md)`, `[`BookmarksStorage`](./index.md)`, `[`SyncableStore`](../../mozilla.components.concept.sync/-syncable-store/index.md)<br>Implementation of the [BookmarksStorage](./index.md) which is backed by a Rust Places lib via [PlacesApi](#). |
