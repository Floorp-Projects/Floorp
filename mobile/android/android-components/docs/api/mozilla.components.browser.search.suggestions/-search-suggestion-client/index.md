[android-components](../../index.md) / [mozilla.components.browser.search.suggestions](../index.md) / [SearchSuggestionClient](./index.md)

# SearchSuggestionClient

`class SearchSuggestionClient` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/search/src/main/java/mozilla/components/browser/search/suggestions/SearchSuggestionClient.kt#L21)

Provides an interface to get search suggestions from a given SearchEngine.

### Exceptions

| Name | Summary |
|---|---|
| [FetchException](-fetch-exception/index.md) | `class FetchException : `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)<br>Exception types for errors caught while getting a list of suggestions |
| [ResponseParserException](-response-parser-exception/index.md) | `class ResponseParserException : `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html) |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SearchSuggestionClient(searchEngine: `[`SearchEngine`](../../mozilla.components.browser.search/-search-engine/index.md)`, fetcher: `[`SearchSuggestionFetcher`](../-search-suggestion-fetcher.md)`)`<br>`SearchSuggestionClient(context: <ERROR CLASS>, searchEngineManager: `[`SearchEngineManager`](../../mozilla.components.browser.search/-search-engine-manager/index.md)`, fetcher: `[`SearchSuggestionFetcher`](../-search-suggestion-fetcher.md)`)` |

### Properties

| Name | Summary |
|---|---|
| [searchEngine](search-engine.md) | `var searchEngine: `[`SearchEngine`](../../mozilla.components.browser.search/-search-engine/index.md)`?` |
| [searchEngineManager](search-engine-manager.md) | `val searchEngineManager: `[`SearchEngineManager`](../../mozilla.components.browser.search/-search-engine-manager/index.md)`?` |

### Functions

| Name | Summary |
|---|---|
| [getSuggestions](get-suggestions.md) | `suspend fun getSuggestions(query: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>?`<br>Gets search suggestions for a given query |
