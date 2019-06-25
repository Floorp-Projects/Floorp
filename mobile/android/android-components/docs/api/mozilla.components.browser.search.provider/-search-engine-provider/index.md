[android-components](../../index.md) / [mozilla.components.browser.search.provider](../index.md) / [SearchEngineProvider](./index.md)

# SearchEngineProvider

`interface SearchEngineProvider` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/search/src/main/java/mozilla/components/browser/search/provider/SearchEngineProvider.kt#L13)

Interface for classes that load search engines from a specific source.

### Functions

| Name | Summary |
|---|---|
| [loadSearchEngines](load-search-engines.md) | `abstract suspend fun loadSearchEngines(context: <ERROR CLASS>): `[`SearchEngineList`](../-search-engine-list/index.md)<br>Load search engines from this provider. |

### Inheritors

| Name | Summary |
|---|---|
| [AssetsSearchEngineProvider](../-assets-search-engine-provider/index.md) | `class AssetsSearchEngineProvider : `[`SearchEngineProvider`](./index.md)<br>SearchEngineProvider implementation to load the included search engines from assets. |
