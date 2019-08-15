[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [EngineSession](index.md) / [saveState](./save-state.md)

# saveState

`abstract fun saveState(): `[`EngineSessionState`](../-engine-session-state/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSession.kt#L398)

Saves and returns the engine state. Engine implementations are not required
to persist the state anywhere else than in the returned map. Engines that
already provide a serialized state can use a single entry in this map to
provide this state. The only requirement is that the same map can be used
to restore the original state. See [restoreState](restore-state.md) and the specific
engine implementation for details.

