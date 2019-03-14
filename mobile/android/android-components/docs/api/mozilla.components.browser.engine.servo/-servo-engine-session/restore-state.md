[android-components](../../index.md) / [mozilla.components.browser.engine.servo](../index.md) / [ServoEngineSession](index.md) / [restoreState](./restore-state.md)

# restoreState

`fun restoreState(state: `[`EngineSessionState`](../../mozilla.components.concept.engine/-engine-session-state/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-servo/src/main/java/mozilla/components/browser/engine/servo/ServoEngineSession.kt#L110)

Overrides [EngineSession.restoreState](../../mozilla.components.concept.engine/-engine-session/restore-state.md)

Restores the engine state as provided by [saveState](../../mozilla.components.concept.engine/-engine-session/save-state.md).

### Parameters

`state` - state retrieved from [saveState](../../mozilla.components.concept.engine/-engine-session/save-state.md)