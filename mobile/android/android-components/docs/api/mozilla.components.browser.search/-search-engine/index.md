[android-components](../../index.md) / [mozilla.components.browser.search](../index.md) / [SearchEngine](./index.md)

# SearchEngine

`class SearchEngine` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/search/src/main/java/mozilla/components/browser/search/SearchEngine.kt#L15)

A data class representing a search engine.

### Properties

| Name | Summary |
|---|---|
| [canProvideSearchSuggestions](can-provide-search-suggestions.md) | `val canProvideSearchSuggestions: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [icon](icon.md) | `val icon: <ERROR CLASS>` |
| [identifier](identifier.md) | `val identifier: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [name](name.md) | `val name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Functions

| Name | Summary |
|---|---|
| [buildSearchUrl](build-search-url.md) | `fun buildSearchUrl(searchTerm: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Builds a URL to search for the given search terms with this search engine. |
| [buildSuggestionsURL](build-suggestions-u-r-l.md) | `fun buildSuggestionsURL(searchTerm: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Builds a URL to get suggestions from this search engine. |
| [toString](to-string.md) | `fun toString(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
