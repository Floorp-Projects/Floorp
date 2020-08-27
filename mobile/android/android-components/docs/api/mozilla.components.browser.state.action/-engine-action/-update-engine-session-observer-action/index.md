[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [EngineAction](../index.md) / [UpdateEngineSessionObserverAction](./index.md)

# UpdateEngineSessionObserverAction

`data class UpdateEngineSessionObserverAction : `[`EngineAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L552)

Updates the [EngineSession.Observer](../../../mozilla.components.concept.engine/-engine-session/-observer/index.md) of the session with the provided [sessionId](session-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UpdateEngineSessionObserverAction(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, engineSessionObserver: `[`Observer`](../../../mozilla.components.concept.engine/-engine-session/-observer/index.md)`)`<br>Updates the [EngineSession.Observer](../../../mozilla.components.concept.engine/-engine-session/-observer/index.md) of the session with the provided [sessionId](session-id.md). |

### Properties

| Name | Summary |
|---|---|
| [engineSessionObserver](engine-session-observer.md) | `val engineSessionObserver: `[`Observer`](../../../mozilla.components.concept.engine/-engine-session/-observer/index.md) |
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
