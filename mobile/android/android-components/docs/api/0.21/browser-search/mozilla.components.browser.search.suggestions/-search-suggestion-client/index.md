---
title: SearchSuggestionClient - 
---

[mozilla.components.browser.search.suggestions](../index.html) / [SearchSuggestionClient](./index.html)

# SearchSuggestionClient

`class SearchSuggestionClient`

Provides an interface to get search suggestions from a given SearchEngine.

### Exceptions

| [FetchException](-fetch-exception/index.html) | `class FetchException : `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)<br>Exception types for errors caught while getting a list of suggestions |
| [ResponseParserException](-response-parser-exception/index.html) | `class ResponseParserException : `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html) |

### Constructors

| [&lt;init&gt;](-init-.html) | `SearchSuggestionClient(searchEngine: `[`SearchEngine`](../../mozilla.components.browser.search/-search-engine/index.html)`, fetcher: `[`SearchSuggestionFetcher`](../-search-suggestion-fetcher.html)`)`<br>Provides an interface to get search suggestions from a given SearchEngine. |

### Functions

| [getSuggestions](get-suggestions.html) | `suspend fun getSuggestions(query: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>?`<br>Gets search suggestions for a given query |

