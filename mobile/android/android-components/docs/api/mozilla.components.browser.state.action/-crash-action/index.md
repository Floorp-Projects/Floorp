[android-components](../../index.md) / [mozilla.components.browser.state.action](../index.md) / [CrashAction](./index.md)

# CrashAction

`sealed class CrashAction : `[`BrowserAction`](../-browser-action.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L561)

[BrowserAction](../-browser-action.md) implementations to react to crashes.

### Types

| Name | Summary |
|---|---|
| [RestoreCrashedSessionAction](-restore-crashed-session-action/index.md) | `data class RestoreCrashedSessionAction : `[`CrashAction`](./index.md)<br>Updates the [SessionState](../../mozilla.components.browser.state.state/-session-state/index.md) of the session with provided ID to mark it as restored. |
| [SessionCrashedAction](-session-crashed-action/index.md) | `data class SessionCrashedAction : `[`CrashAction`](./index.md)<br>Updates the [SessionState](../../mozilla.components.browser.state.state/-session-state/index.md) of the session with provided ID to mark it as crashed. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [RestoreCrashedSessionAction](-restore-crashed-session-action/index.md) | `data class RestoreCrashedSessionAction : `[`CrashAction`](./index.md)<br>Updates the [SessionState](../../mozilla.components.browser.state.state/-session-state/index.md) of the session with provided ID to mark it as restored. |
| [SessionCrashedAction](-session-crashed-action/index.md) | `data class SessionCrashedAction : `[`CrashAction`](./index.md)<br>Updates the [SessionState](../../mozilla.components.browser.state.state/-session-state/index.md) of the session with provided ID to mark it as crashed. |
