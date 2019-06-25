[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [EngineSessionState](index.md) / [toJSON](./to-j-s-o-n.md)

# toJSON

`abstract fun toJSON(): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSessionState.kt#L21)

Create a JSON representation of this state that can be saved to disk.

When reading JSON from disk [Engine.createSessionState](../-engine/create-session-state.md) can be used to turn it back into an [EngineSessionState](index.md)
instance.

