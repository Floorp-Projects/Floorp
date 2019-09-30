[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesHistoryStorage](index.md) / [getVisited](./get-visited.md)

# getVisited

`open suspend fun getVisited(uris: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesHistoryStorage.kt#L60)

Overrides [HistoryStorage.getVisited](../../mozilla.components.concept.storage/-history-storage/get-visited.md)

Maps a list of page URIs to a list of booleans indicating if each URI was visited.

### Parameters

`uris` - a list of page URIs about which "visited" information is being requested.

**Return**
A list of booleans indicating visited status of each
corresponding page URI from [uris](../../mozilla.components.concept.storage/-history-storage/get-visited.md#mozilla.components.concept.storage.HistoryStorage$getVisited(kotlin.collections.List((kotlin.String)))/uris).

`open suspend fun getVisited(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesHistoryStorage.kt#L64)

Overrides [HistoryStorage.getVisited](../../mozilla.components.concept.storage/-history-storage/get-visited.md)

Retrieves a list of all visited pages.

**Return**
A list of all visited page URIs.

