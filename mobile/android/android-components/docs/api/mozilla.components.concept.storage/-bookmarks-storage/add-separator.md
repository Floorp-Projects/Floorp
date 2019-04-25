[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [BookmarksStorage](index.md) / [addSeparator](./add-separator.md)

# addSeparator

`abstract suspend fun addSeparator(parentGuid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/BookmarksStorage.kt#L80)

Adds a new bookmark separator to a given node.

Sync behavior: will add new separator to remote devices.

### Parameters

`parentGuid` - The parent guid of the new node.

`position` - The optional position to add the new node or null to append.

**Return**
The guid of the newly inserted bookmark item.

