[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [ContentAction](../index.md) / [UpdateForwardNavigationStateAction](./index.md)

# UpdateForwardNavigationStateAction

`data class UpdateForwardNavigationStateAction : `[`ContentAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L289)

Updates the [ContentState](../../../mozilla.components.browser.state.state/-content-state/index.md) of the given [sessionId](session-id.md) to indicate whether or not a forward navigation is possible.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UpdateForwardNavigationStateAction(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, canGoForward: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`)`<br>Updates the [ContentState](../../../mozilla.components.browser.state.state/-content-state/index.md) of the given [sessionId](session-id.md) to indicate whether or not a forward navigation is possible. |

### Properties

| Name | Summary |
|---|---|
| [canGoForward](can-go-forward.md) | `val canGoForward: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
