[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [SearchAction](../index.md) / [AddSearchEngineListAction](./index.md)

# AddSearchEngineListAction

`data class AddSearchEngineListAction : `[`SearchAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L627)

Updates [BrowserState.search](../../../mozilla.components.browser.state.state/-browser-state/search.md) to add/modify [SearchState.searchEngines](../../../mozilla.components.browser.state.state/-search-state/search-engines.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AddSearchEngineListAction(searchEngineList: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`SearchEngine`](../../../mozilla.components.browser.state.search/-search-engine/index.md)`>)`<br>Updates [BrowserState.search](../../../mozilla.components.browser.state.state/-browser-state/search.md) to add/modify [SearchState.searchEngines](../../../mozilla.components.browser.state.state/-search-state/search-engines.md). |

### Properties

| Name | Summary |
|---|---|
| [searchEngineList](search-engine-list.md) | `val searchEngineList: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`SearchEngine`](../../../mozilla.components.browser.state.search/-search-engine/index.md)`>` |
