[android-components](../../index.md) / [mozilla.components.browser.search.provider](../index.md) / [SearchEngineList](./index.md)

# SearchEngineList

`data class SearchEngineList` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/search/src/main/java/mozilla/components/browser/search/provider/SearchEngineProvider.kt#L24)

Data class providing an ordered list of search engines and a default search engine from a
specific source.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SearchEngineList(list: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`SearchEngine`](../../mozilla.components.browser.search/-search-engine/index.md)`>, default: `[`SearchEngine`](../../mozilla.components.browser.search/-search-engine/index.md)`?)`<br>Data class providing an ordered list of search engines and a default search engine from a specific source. |

### Properties

| Name | Summary |
|---|---|
| [default](default.md) | `val default: `[`SearchEngine`](../../mozilla.components.browser.search/-search-engine/index.md)`?`<br>The default search engine if the user has no preference. |
| [list](list.md) | `val list: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`SearchEngine`](../../mozilla.components.browser.search/-search-engine/index.md)`>`<br>An ordered list of search engines. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
