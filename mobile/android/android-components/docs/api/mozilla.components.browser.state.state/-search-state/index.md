[android-components](../../index.md) / [mozilla.components.browser.state.state](../index.md) / [SearchState](./index.md)

# SearchState

`data class SearchState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/SearchState.kt#L15)

Value type that represents the state of search.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SearchState(searchEngines: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`SearchEngine`](../../mozilla.components.browser.state.search/-search-engine/index.md)`> = emptyMap(), defaultSearchEngineId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null)`<br>Value type that represents the state of search. |

### Properties

| Name | Summary |
|---|---|
| [defaultSearchEngineId](default-search-engine-id.md) | `val defaultSearchEngineId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>The ID of default [SearchEngine](../../mozilla.components.browser.state.search/-search-engine/index.md) |
| [searchEngines](search-engines.md) | `val searchEngines: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`SearchEngine`](../../mozilla.components.browser.state.search/-search-engine/index.md)`>`<br>The map of search engines (bundled and custom) to their respective IDs. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
