---
title: SearchEngine - 
---

[mozilla.components.browser.search](../index.html) / [SearchEngine](./index.html)

# SearchEngine

`class SearchEngine`

A data class representing a search engine.

### Properties

| [canProvideSearchSuggestions](can-provide-search-suggestions.html) | `val canProvideSearchSuggestions: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [icon](icon.html) | `val icon: Bitmap` |
| [identifier](identifier.html) | `val identifier: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [name](name.html) | `val name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Functions

| [buildSearchUrl](build-search-url.html) | `fun buildSearchUrl(searchTerm: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Builds a URL to search for the given search terms with this search engine. |
| [buildSuggestionsURL](build-suggestions-u-r-l.html) | `fun buildSuggestionsURL(searchTerm: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Builds a URL to get suggestions from this search engine. |

