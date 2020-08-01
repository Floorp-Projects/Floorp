[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [EngineSessionState](./index.md)

# EngineSessionState

`interface EngineSessionState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSessionState.kt#L14)

The state of an [EngineSession](../-engine-session/index.md). An instance can be obtained from [EngineSession.saveState](../-engine-session/save-state.md). Creating a new
[EngineSession](../-engine-session/index.md) and calling [EngineSession.restoreState](../-engine-session/restore-state.md) with the same state instance should restore the previous
session.

### Functions

| Name | Summary |
|---|---|
| [toJSON](to-j-s-o-n.md) | `abstract fun toJSON(): <ERROR CLASS>`<br>Create a JSON representation of this state that can be saved to disk. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [GeckoEngineSessionState](../../mozilla.components.browser.engine.gecko/-gecko-engine-session-state/index.md) | `class GeckoEngineSessionState : `[`EngineSessionState`](./index.md) |
| [SystemEngineSessionState](../../mozilla.components.browser.engine.system/-system-engine-session-state/index.md) | `class SystemEngineSessionState : `[`EngineSessionState`](./index.md) |
