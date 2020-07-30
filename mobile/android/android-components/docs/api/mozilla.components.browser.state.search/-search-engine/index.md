[android-components](../../index.md) / [mozilla.components.browser.state.search](../index.md) / [SearchEngine](./index.md)

# SearchEngine

`data class SearchEngine` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/search/SearchEngine.kt#L19)

A data class representing a search engine.

### Types

| Name | Summary |
|---|---|
| [Type](-type/index.md) | `enum class Type`<br>A enum class representing a search engine type. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SearchEngine(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, icon: <ERROR CLASS>, type: `[`Type`](-type/index.md)`, resultUrls: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = emptyList(), suggestUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null)`<br>A data class representing a search engine. |

### Properties

| Name | Summary |
|---|---|
| [icon](icon.md) | `val icon: <ERROR CLASS>`<br>the icon of this search engine. |
| [id](id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the ID of this search engine. |
| [name](name.md) | `val name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the name of this search engine. |
| [resultUrls](result-urls.md) | `val resultUrls: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>the list of the queried suggestions result urls. |
| [suggestUrl](suggest-url.md) | `val suggestUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>the search suggestion url. |
| [type](type.md) | `val type: `[`Type`](-type/index.md)<br>the type of this search engine. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
