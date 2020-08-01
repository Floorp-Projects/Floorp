[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [ContentAction](../index.md) / [UpdateWindowRequestAction](./index.md)

# UpdateWindowRequestAction

`data class UpdateWindowRequestAction : `[`ContentAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L238)

Updates the [WindowRequest](../../../mozilla.components.concept.engine.window/-window-request/index.md) of the [ContentState](../../../mozilla.components.browser.state.state/-content-state/index.md) with the given [sessionId](session-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UpdateWindowRequestAction(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, windowRequest: `[`WindowRequest`](../../../mozilla.components.concept.engine.window/-window-request/index.md)`)`<br>Updates the [WindowRequest](../../../mozilla.components.concept.engine.window/-window-request/index.md) of the [ContentState](../../../mozilla.components.browser.state.state/-content-state/index.md) with the given [sessionId](session-id.md). |

### Properties

| Name | Summary |
|---|---|
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [windowRequest](window-request.md) | `val windowRequest: `[`WindowRequest`](../../../mozilla.components.concept.engine.window/-window-request/index.md) |
