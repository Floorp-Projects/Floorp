[android-components](../../../index.md) / [mozilla.components.browser.session.action](../../index.md) / [SessionAction](../index.md) / [UpdateProgressAction](./index.md)

# UpdateProgressAction

`data class UpdateProgressAction : `[`SessionAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/session/action/BrowserAction.kt#L49)

Updates the progress of the [SessionState](../../../mozilla.components.browser.session.state/-session-state/index.md) with the given [sessionId](session-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UpdateProgressAction(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, progress: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`)`<br>Updates the progress of the [SessionState](../../../mozilla.components.browser.session.state/-session-state/index.md) with the given [sessionId](session-id.md). |

### Properties

| Name | Summary |
|---|---|
| [progress](progress.md) | `val progress: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
