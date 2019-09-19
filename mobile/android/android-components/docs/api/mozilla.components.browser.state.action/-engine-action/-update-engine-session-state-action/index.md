[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [EngineAction](../index.md) / [UpdateEngineSessionStateAction](./index.md)

# UpdateEngineSessionStateAction

`data class UpdateEngineSessionStateAction : `[`EngineAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L263)

Updates the [EngineSessionState](../../../mozilla.components.concept.engine/-engine-session-state/index.md) of the session with the provided [sessionId](session-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UpdateEngineSessionStateAction(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, engineSessionState: `[`EngineSessionState`](../../../mozilla.components.concept.engine/-engine-session-state/index.md)`)`<br>Updates the [EngineSessionState](../../../mozilla.components.concept.engine/-engine-session-state/index.md) of the session with the provided [sessionId](session-id.md). |

### Properties

| Name | Summary |
|---|---|
| [engineSessionState](engine-session-state.md) | `val engineSessionState: `[`EngineSessionState`](../../../mozilla.components.concept.engine/-engine-session-state/index.md) |
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
