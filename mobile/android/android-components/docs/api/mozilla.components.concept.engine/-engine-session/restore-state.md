[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [EngineSession](index.md) / [restoreState](./restore-state.md)

# restoreState

`abstract fun restoreState(state: `[`EngineSessionState`](../-engine-session-state/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSession.kt#L405)

Restores the engine state as provided by [saveState](save-state.md).

### Parameters

`state` - state retrieved from [saveState](save-state.md)