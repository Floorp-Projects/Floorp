[android-components](../../index.md) / [mozilla.components.feature.tab.collections](../index.md) / [TabCollectionStorage](index.md) / [getCollections](./get-collections.md)

# getCollections

`fun getCollections(limit: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 20): Flow<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TabCollection`](../-tab-collection/index.md)`>>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tab-collections/src/main/java/mozilla/components/feature/tab/collections/TabCollectionStorage.kt#L115)

Returns the last [TabCollection](../-tab-collection/index.md) instances (up to [limit](get-collections.md#mozilla.components.feature.tab.collections.TabCollectionStorage$getCollections(kotlin.Int)/limit)) as a [Flow](#) list.

### Parameters

`limit` - (Optional) Maximum number of [TabCollection](../-tab-collection/index.md) instances that should be returned.