[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [SearchAction](../index.md) / [RemoveCustomSearchEngineAction](./index.md)

# RemoveCustomSearchEngineAction

`data class RemoveCustomSearchEngineAction : `[`SearchAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L637)

Updates [BrowserState.search](../../../mozilla.components.browser.state.state/-browser-state/search.md) to remove a custom [SearchEngine](../../../mozilla.components.browser.state.search/-search-engine/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `RemoveCustomSearchEngineAction(searchEngineId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>Updates [BrowserState.search](../../../mozilla.components.browser.state.state/-browser-state/search.md) to remove a custom [SearchEngine](../../../mozilla.components.browser.state.search/-search-engine/index.md). |

### Properties

| Name | Summary |
|---|---|
| [searchEngineId](search-engine-id.md) | `val searchEngineId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
