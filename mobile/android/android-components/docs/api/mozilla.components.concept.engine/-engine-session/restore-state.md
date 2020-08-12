[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [EngineSession](index.md) / [restoreState](./restore-state.md)

# restoreState

`abstract fun restoreState(state: `[`EngineSessionState`](../-engine-session-state/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSession.kt#L523)

Restores the engine state as provided by [saveState](save-state.md).

### Parameters

`state` - state retrieved from [saveState](save-state.md)

**Return**
true if the engine session has successfully been restored with the provided state,
false otherwise.

