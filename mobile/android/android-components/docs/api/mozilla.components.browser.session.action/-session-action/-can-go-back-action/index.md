[android-components](../../../index.md) / [mozilla.components.browser.session.action](../../index.md) / [SessionAction](../index.md) / [CanGoBackAction](./index.md)

# CanGoBackAction

`data class CanGoBackAction : `[`SessionAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/session/action/BrowserAction.kt#L53)

Updates the "canGoBack" state of the [SessionState](../../../mozilla.components.browser.session.state/-session-state/index.md) with the given [sessionId](session-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `CanGoBackAction(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, canGoBack: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`)`<br>Updates the "canGoBack" state of the [SessionState](../../../mozilla.components.browser.session.state/-session-state/index.md) with the given [sessionId](session-id.md). |

### Properties

| Name | Summary |
|---|---|
| [canGoBack](can-go-back.md) | `val canGoBack: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
