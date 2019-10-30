[android-components](../../index.md) / [mozilla.components.browser.search.provider.filter](../index.md) / [SearchEngineFilter](./index.md)

# SearchEngineFilter

`interface SearchEngineFilter` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/search/src/main/java/mozilla/components/browser/search/provider/filter/SearchEngineFilter.kt#L14)

Interface for classes that want to filter the list of search engines a SearchEngineProvider
implementation loads.

### Functions

| Name | Summary |
|---|---|
| [filter](filter.md) | `abstract fun filter(context: <ERROR CLASS>, searchEngine: `[`SearchEngine`](../../mozilla.components.browser.search/-search-engine/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if the given search engine should be returned by the provider or false if this search engine should be ignored. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
