[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [ContentAction](../index.md) / [UpdateSearchRequestAction](./index.md)

# UpdateSearchRequestAction

`data class UpdateSearchRequestAction : `[`ContentAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L248)

Updates the [SearchRequest](../../../mozilla.components.concept.engine.search/-search-request/index.md) of the [ContentState](../../../mozilla.components.browser.state.state/-content-state/index.md) with the given [sessionId](session-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UpdateSearchRequestAction(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, searchRequest: `[`SearchRequest`](../../../mozilla.components.concept.engine.search/-search-request/index.md)`)`<br>Updates the [SearchRequest](../../../mozilla.components.concept.engine.search/-search-request/index.md) of the [ContentState](../../../mozilla.components.browser.state.state/-content-state/index.md) with the given [sessionId](session-id.md). |

### Properties

| Name | Summary |
|---|---|
| [searchRequest](search-request.md) | `val searchRequest: `[`SearchRequest`](../../../mozilla.components.concept.engine.search/-search-request/index.md) |
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
