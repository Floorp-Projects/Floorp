[android-components](../../index.md) / [mozilla.components.browser.storage.memory](../index.md) / [InMemoryHistoryStorage](index.md) / [getSuggestions](./get-suggestions.md)

# getSuggestions

`fun getSuggestions(query: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, limit: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`SearchResult`](../../mozilla.components.concept.storage/-search-result/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-memory/src/main/java/mozilla/components/browser/storage/memory/InMemoryHistoryStorage.kt#L87)

Overrides [HistoryStorage.getSuggestions](../../mozilla.components.concept.storage/-history-storage/get-suggestions.md)

Retrieves suggestions matching the [query](../../mozilla.components.concept.storage/-history-storage/get-suggestions.md#mozilla.components.concept.storage.HistoryStorage$getSuggestions(kotlin.String, kotlin.Int)/query).

### Parameters

`query` - A query by which to search the underlying store.

**Return**
A List of [SearchResult](../../mozilla.components.concept.storage/-search-result/index.md) matching the query, in no particular order.

