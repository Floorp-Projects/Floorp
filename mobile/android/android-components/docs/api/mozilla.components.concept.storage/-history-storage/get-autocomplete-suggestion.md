[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [HistoryStorage](index.md) / [getAutocompleteSuggestion](./get-autocomplete-suggestion.md)

# getAutocompleteSuggestion

`abstract fun getAutocompleteSuggestion(query: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`HistoryAutocompleteResult`](../-history-autocomplete-result/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/HistoryStorage.kt#L92)

Retrieves domain suggestions which best match the [query](get-autocomplete-suggestion.md#mozilla.components.concept.storage.HistoryStorage$getAutocompleteSuggestion(kotlin.String)/query).

### Parameters

`query` - A query by which to search the underlying store.

**Return**
An optional domain URL which best matches the query.

