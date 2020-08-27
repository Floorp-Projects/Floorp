[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [EngineAction](../index.md) / [SuspendEngineSessionAction](./index.md)

# SuspendEngineSessionAction

`data class SuspendEngineSessionAction : `[`EngineAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L532)

Suspends the [EngineSession](../../../mozilla.components.concept.engine/-engine-session/index.md) of the session with the provided [sessionId](session-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SuspendEngineSessionAction(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>Suspends the [EngineSession](../../../mozilla.components.concept.engine/-engine-session/index.md) of the session with the provided [sessionId](session-id.md). |

### Properties

| Name | Summary |
|---|---|
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
