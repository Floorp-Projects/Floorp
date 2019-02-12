[android-components](../../index.md) / [mozilla.components.browser.engine.gecko](../index.md) / [GeckoEngineSessionState](index.md) / [toJSON](./to-j-s-o-n.md)

# toJSON

`fun toJSON(): `[`JSONObject`](https://developer.android.com/reference/org/json/JSONObject.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/GeckoEngineSessionState.kt#L17)

Overrides [EngineSessionState.toJSON](../../mozilla.components.concept.engine/-engine-session-state/to-j-s-o-n.md)

Create a JSON representation of this state that can be saved to disk.

When reading JSON from disk [Engine.createSessionState](#) can be used to turn it back into an [EngineSessionState](../../mozilla.components.concept.engine/-engine-session-state/index.md)
instance.

