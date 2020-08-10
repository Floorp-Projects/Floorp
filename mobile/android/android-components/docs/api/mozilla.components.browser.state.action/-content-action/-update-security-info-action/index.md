[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [ContentAction](../index.md) / [UpdateSecurityInfoAction](./index.md)

# UpdateSecurityInfoAction

`data class UpdateSecurityInfoAction : `[`ContentAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L183)

Updates the [SecurityInfoState](../../../mozilla.components.browser.state.state/-security-info-state/index.md) of the [ContentState](../../../mozilla.components.browser.state.state/-content-state/index.md) with the given [sessionId](session-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UpdateSecurityInfoAction(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, securityInfo: `[`SecurityInfoState`](../../../mozilla.components.browser.state.state/-security-info-state/index.md)`)`<br>Updates the [SecurityInfoState](../../../mozilla.components.browser.state.state/-security-info-state/index.md) of the [ContentState](../../../mozilla.components.browser.state.state/-content-state/index.md) with the given [sessionId](session-id.md). |

### Properties

| Name | Summary |
|---|---|
| [securityInfo](security-info.md) | `val securityInfo: `[`SecurityInfoState`](../../../mozilla.components.browser.state.state/-security-info-state/index.md) |
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
