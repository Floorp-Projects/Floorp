[android-components](../../index.md) / [mozilla.components.browser.state.action](../index.md) / [SearchAction](./index.md)

# SearchAction

`sealed class SearchAction : `[`BrowserAction`](../-browser-action.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L623)

[BrowserAction](../-browser-action.md) implementations related to updating search engines in [SearchState](../../mozilla.components.browser.state.state/-search-state/index.md).

### Types

| Name | Summary |
|---|---|
| [AddSearchEngineListAction](-add-search-engine-list-action/index.md) | `data class AddSearchEngineListAction : `[`SearchAction`](./index.md)<br>Updates [BrowserState.search](../../mozilla.components.browser.state.state/-browser-state/search.md) to add/modify [SearchState.searchEngines](../../mozilla.components.browser.state.state/-search-state/search-engines.md). |
| [RemoveCustomSearchEngineAction](-remove-custom-search-engine-action/index.md) | `data class RemoveCustomSearchEngineAction : `[`SearchAction`](./index.md)<br>Updates [BrowserState.search](../../mozilla.components.browser.state.state/-browser-state/search.md) to remove a custom [SearchEngine](../../mozilla.components.browser.state.search/-search-engine/index.md). |
| [SetCustomSearchEngineAction](-set-custom-search-engine-action/index.md) | `data class SetCustomSearchEngineAction : `[`SearchAction`](./index.md)<br>Updates [BrowserState.search](../../mozilla.components.browser.state.state/-browser-state/search.md) to add/modify a custom [SearchEngine](../../mozilla.components.browser.state.search/-search-engine/index.md). |
| [SetDefaultSearchEngineAction](-set-default-search-engine-action/index.md) | `data class SetDefaultSearchEngineAction : `[`SearchAction`](./index.md)<br>Updates [BrowserState.search](../../mozilla.components.browser.state.state/-browser-state/search.md) to update [SearchState.defaultSearchEngineId](../../mozilla.components.browser.state.state/-search-state/default-search-engine-id.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [AddSearchEngineListAction](-add-search-engine-list-action/index.md) | `data class AddSearchEngineListAction : `[`SearchAction`](./index.md)<br>Updates [BrowserState.search](../../mozilla.components.browser.state.state/-browser-state/search.md) to add/modify [SearchState.searchEngines](../../mozilla.components.browser.state.state/-search-state/search-engines.md). |
| [RemoveCustomSearchEngineAction](-remove-custom-search-engine-action/index.md) | `data class RemoveCustomSearchEngineAction : `[`SearchAction`](./index.md)<br>Updates [BrowserState.search](../../mozilla.components.browser.state.state/-browser-state/search.md) to remove a custom [SearchEngine](../../mozilla.components.browser.state.search/-search-engine/index.md). |
| [SetCustomSearchEngineAction](-set-custom-search-engine-action/index.md) | `data class SetCustomSearchEngineAction : `[`SearchAction`](./index.md)<br>Updates [BrowserState.search](../../mozilla.components.browser.state.state/-browser-state/search.md) to add/modify a custom [SearchEngine](../../mozilla.components.browser.state.search/-search-engine/index.md). |
| [SetDefaultSearchEngineAction](-set-default-search-engine-action/index.md) | `data class SetDefaultSearchEngineAction : `[`SearchAction`](./index.md)<br>Updates [BrowserState.search](../../mozilla.components.browser.state.state/-browser-state/search.md) to update [SearchState.defaultSearchEngineId](../../mozilla.components.browser.state.state/-search-state/default-search-engine-id.md). |
