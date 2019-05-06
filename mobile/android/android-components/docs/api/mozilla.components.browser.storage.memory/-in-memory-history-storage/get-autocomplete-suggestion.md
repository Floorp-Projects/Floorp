[android-components](../../index.md) / [mozilla.components.browser.storage.memory](../index.md) / [InMemoryHistoryStorage](index.md) / [getAutocompleteSuggestion](./get-autocomplete-suggestion.md)

# getAutocompleteSuggestion

`fun getAutocompleteSuggestion(query: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`HistoryAutocompleteResult`](../../mozilla.components.concept.storage/-history-autocomplete-result/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-memory/src/main/java/mozilla/components/browser/storage/memory/InMemoryHistoryStorage.kt#L114)

Overrides [HistoryStorage.getAutocompleteSuggestion](../../mozilla.components.concept.storage/-history-storage/get-autocomplete-suggestion.md)

Retrieves domain suggestions which best match the [query](../../mozilla.components.concept.storage/-history-storage/get-autocomplete-suggestion.md#mozilla.components.concept.storage.HistoryStorage$getAutocompleteSuggestion(kotlin.String)/query).

### Parameters

`query` - A query by which to search the underlying store.

**Return**
An optional domain URL which best matches the query.

