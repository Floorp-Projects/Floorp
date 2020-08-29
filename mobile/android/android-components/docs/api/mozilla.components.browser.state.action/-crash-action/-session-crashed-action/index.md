[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [CrashAction](../index.md) / [SessionCrashedAction](./index.md)

# SessionCrashedAction

`data class SessionCrashedAction : `[`CrashAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L581)

Updates the [SessionState](../../../mozilla.components.browser.state.state/-session-state/index.md) of the session with provided ID to mark it as crashed.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SessionCrashedAction(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>Updates the [SessionState](../../../mozilla.components.browser.state.state/-session-state/index.md) of the session with provided ID to mark it as crashed. |

### Properties

| Name | Summary |
|---|---|
| [tabId](tab-id.md) | `val tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
