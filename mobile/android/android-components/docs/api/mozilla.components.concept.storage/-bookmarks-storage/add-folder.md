[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [BookmarksStorage](index.md) / [addFolder](./add-folder.md)

# addFolder

`abstract suspend fun addFolder(parentGuid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`? = null): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/BookmarksStorage.kt#L69)

Adds a new bookmark folder to a given node.

Sync behavior: will add new separator to remote devices.

### Parameters

`parentGuid` - The parent guid of the new node.

`title` - The title of the bookmark folder to add.

`position` - The optional position to add the new node or null to append.

**Return**
The guid of the newly inserted bookmark item.

