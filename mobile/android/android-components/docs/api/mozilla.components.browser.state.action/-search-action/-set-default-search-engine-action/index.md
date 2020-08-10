[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [SearchAction](../index.md) / [SetDefaultSearchEngineAction](./index.md)

# SetDefaultSearchEngineAction

`data class SetDefaultSearchEngineAction : `[`SearchAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L642)

Updates [BrowserState.search](../../../mozilla.components.browser.state.state/-browser-state/search.md) to update [SearchState.defaultSearchEngineId](../../../mozilla.components.browser.state.state/-search-state/default-search-engine-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SetDefaultSearchEngineAction(searchEngineId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>Updates [BrowserState.search](../../../mozilla.components.browser.state.state/-browser-state/search.md) to update [SearchState.defaultSearchEngineId](../../../mozilla.components.browser.state.state/-search-state/default-search-engine-id.md). |

### Properties

| Name | Summary |
|---|---|
| [searchEngineId](search-engine-id.md) | `val searchEngineId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
