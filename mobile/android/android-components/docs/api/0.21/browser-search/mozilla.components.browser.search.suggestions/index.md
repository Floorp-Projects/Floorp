---
title: mozilla.components.browser.search.suggestions - 
---

[mozilla.components.browser.search.suggestions](./index.html)

## Package mozilla.components.browser.search.suggestions

### Types

| [SearchSuggestionClient](-search-suggestion-client/index.html) | `class SearchSuggestionClient`<br>Provides an interface to get search suggestions from a given SearchEngine. |

### Type Aliases

| [JSONResponse](-j-s-o-n-response.html) | `typealias JSONResponse = `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The Parser is a function that takes a JSON Response and maps it to a Suggestion list. |
| [ResponseParser](-response-parser.html) | `typealias ResponseParser = (`[`JSONResponse`](-j-s-o-n-response.html)`) -> `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` |
| [SearchSuggestionFetcher](-search-suggestion-fetcher.html) | `typealias SearchSuggestionFetcher = suspend (url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Async function responsible for taking a URL and returning the results |

