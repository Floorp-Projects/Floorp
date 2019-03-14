[android-components](../../index.md) / [mozilla.components.browser.engine.servo](../index.md) / [ServoEngineSession](index.md) / [saveState](./save-state.md)

# saveState

`fun saveState(): `[`EngineSessionState`](../../mozilla.components.concept.engine/-engine-session-state/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-servo/src/main/java/mozilla/components/browser/engine/servo/ServoEngineSession.kt#L106)

Overrides [EngineSession.saveState](../../mozilla.components.concept.engine/-engine-session/save-state.md)

Saves and returns the engine state. Engine implementations are not required
to persist the state anywhere else than in the returned map. Engines that
already provide a serialized state can use a single entry in this map to
provide this state. The only requirement is that the same map can be used
to restore the original state. See [restoreState](../../mozilla.components.concept.engine/-engine-session/restore-state.md) and the specific
engine implementation for details.

