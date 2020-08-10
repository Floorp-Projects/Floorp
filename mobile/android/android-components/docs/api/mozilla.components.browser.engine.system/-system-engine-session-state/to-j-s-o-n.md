[android-components](../../index.md) / [mozilla.components.browser.engine.system](../index.md) / [SystemEngineSessionState](index.md) / [toJSON](./to-j-s-o-n.md)

# toJSON

`fun toJSON(): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/SystemEngineSessionState.kt#L14)

Overrides [EngineSessionState.toJSON](../../mozilla.components.concept.engine/-engine-session-state/to-j-s-o-n.md)

Create a JSON representation of this state that can be saved to disk.

When reading JSON from disk [Engine.createSessionState](../../mozilla.components.concept.engine/-engine/create-session-state.md) can be used to turn it back into an [EngineSessionState](../../mozilla.components.concept.engine/-engine-session-state/index.md)
instance.

