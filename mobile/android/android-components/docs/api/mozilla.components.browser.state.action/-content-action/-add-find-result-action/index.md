[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [ContentAction](../index.md) / [AddFindResultAction](./index.md)

# AddFindResultAction

`data class AddFindResultAction : `[`ContentAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L210)

Adds a [FindResultState](../../../mozilla.components.browser.state.state.content/-find-result-state/index.md) to the [ContentState](../../../mozilla.components.browser.state.state/-content-state/index.md) with the given [sessionId](session-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AddFindResultAction(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, findResult: `[`FindResultState`](../../../mozilla.components.browser.state.state.content/-find-result-state/index.md)`)`<br>Adds a [FindResultState](../../../mozilla.components.browser.state.state.content/-find-result-state/index.md) to the [ContentState](../../../mozilla.components.browser.state.state/-content-state/index.md) with the given [sessionId](session-id.md). |

### Properties

| Name | Summary |
|---|---|
| [findResult](find-result.md) | `val findResult: `[`FindResultState`](../../../mozilla.components.browser.state.state.content/-find-result-state/index.md) |
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
