[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [ContentAction](../index.md) / [UpdateLoadingStateAction](./index.md)

# UpdateLoadingStateAction

`data class UpdateLoadingStateAction : `[`ContentAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L173)

Updates the loading state of the [ContentState](../../../mozilla.components.browser.state.state/-content-state/index.md) with the given [sessionId](session-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UpdateLoadingStateAction(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, loading: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`)`<br>Updates the loading state of the [ContentState](../../../mozilla.components.browser.state.state/-content-state/index.md) with the given [sessionId](session-id.md). |

### Properties

| Name | Summary |
|---|---|
| [loading](loading.md) | `val loading: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
