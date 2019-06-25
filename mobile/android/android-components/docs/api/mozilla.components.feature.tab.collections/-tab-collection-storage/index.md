[android-components](../../index.md) / [mozilla.components.feature.tab.collections](../index.md) / [TabCollectionStorage](./index.md)

# TabCollectionStorage

`class TabCollectionStorage` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tab-collections/src/main/java/mozilla/components/feature/tab/collections/TabCollectionStorage.kt#L26)

A storage implementation that saves snapshots of tabs / sessions in named collections.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TabCollectionStorage(context: <ERROR CLASS>, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, filesDir: `[`File`](https://developer.android.com/reference/java/io/File.html)` = context.filesDir)`<br>A storage implementation that saves snapshots of tabs / sessions in named collections. |

### Functions

| Name | Summary |
|---|---|
| [addTabsToCollection](add-tabs-to-collection.md) | `fun addTabsToCollection(collection: `[`TabCollection`](../-tab-collection/index.md)`, sessions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Session`](../../mozilla.components.browser.session/-session/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds the state of the given [Session](../../mozilla.components.browser.session/-session/index.md)s to the [TabCollection](../-tab-collection/index.md). |
| [createCollection](create-collection.md) | `fun createCollection(title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, sessions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Session`](../../mozilla.components.browser.session/-session/index.md)`> = emptyList()): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Creates a new [TabCollection](../-tab-collection/index.md) and save the state of the given [Session](../../mozilla.components.browser.session/-session/index.md)s in it. |
| [getCollections](get-collections.md) | `fun getCollections(limit: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 20): LiveData<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TabCollection`](../-tab-collection/index.md)`>>`<br>Returns the last [TabCollection](../-tab-collection/index.md) instances (up to [limit](get-collections.md#mozilla.components.feature.tab.collections.TabCollectionStorage$getCollections(kotlin.Int)/limit)) as a [LiveData](#) list. |
| [getCollectionsPaged](get-collections-paged.md) | `fun getCollectionsPaged(): Factory<`[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, `[`TabCollection`](../-tab-collection/index.md)`>`<br>Returns all [TabCollection](../-tab-collection/index.md)s as a [DataSource.Factory](#). |
| [getTabCollectionsCount](get-tab-collections-count.md) | `fun getTabCollectionsCount(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Returns the number of tab collections. |
| [removeAllCollections](remove-all-collections.md) | `fun removeAllCollections(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes all collections and all tabs. |
| [removeCollection](remove-collection.md) | `fun removeCollection(collection: `[`TabCollection`](../-tab-collection/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes a collection and all its tabs. |
| [removeTabFromCollection](remove-tab-from-collection.md) | `fun removeTabFromCollection(collection: `[`TabCollection`](../-tab-collection/index.md)`, tab: `[`Tab`](../-tab/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes the given [Tab](../-tab/index.md) from the [TabCollection](../-tab-collection/index.md). |
| [renameCollection](rename-collection.md) | `fun renameCollection(collection: `[`TabCollection`](../-tab-collection/index.md)`, title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Renames a collection. |
