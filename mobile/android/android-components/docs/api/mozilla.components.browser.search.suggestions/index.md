[android-components](../index.md) / [mozilla.components.browser.search.suggestions](./index.md)

## Package mozilla.components.browser.search.suggestions

### Types

| Name | Summary |
|---|---|
| [SearchSuggestionClient](-search-suggestion-client/index.md) | `class SearchSuggestionClient`<br>Provides an interface to get search suggestions from a given SearchEngine. |

### Type Aliases

| Name | Summary |
|---|---|
| [JSONResponse](-j-s-o-n-response.md) | `typealias JSONResponse = `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The Parser is a function that takes a JSON Response and maps it to a Suggestion list. |
| [ResponseParser](-response-parser.md) | `typealias ResponseParser = (`[`JSONResponse`](-j-s-o-n-response.md)`) -> `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` |
| [SearchSuggestionFetcher](-search-suggestion-fetcher.md) | `typealias SearchSuggestionFetcher = suspend (url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Async function responsible for taking a URL and returning the results |
