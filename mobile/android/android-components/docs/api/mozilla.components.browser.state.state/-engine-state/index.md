[android-components](../../index.md) / [mozilla.components.browser.state.state](../index.md) / [EngineState](./index.md)

# EngineState

`data class EngineState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/EngineState.kt#L17)

Value type that holds the browser engine state of a session.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `EngineState(engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`? = null, engineSessionState: `[`EngineSessionState`](../../mozilla.components.concept.engine/-engine-session-state/index.md)`? = null)`<br>Value type that holds the browser engine state of a session. |

### Properties

| Name | Summary |
|---|---|
| [engineSession](engine-session.md) | `val engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?`<br>the engine's representation of this session. |
| [engineSessionState](engine-session-state.md) | `val engineSessionState: `[`EngineSessionState`](../../mozilla.components.concept.engine/-engine-session-state/index.md)`?`<br>serializable and restorable state of an engine session, see [EngineSession.saveState](../../mozilla.components.concept.engine/-engine-session/save-state.md) and [EngineSession.restoreState](../../mozilla.components.concept.engine/-engine-session/restore-state.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
