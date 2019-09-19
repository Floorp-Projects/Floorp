[android-components](../../index.md) / [mozilla.components.concept.engine.window](../index.md) / [WindowRequest](index.md) / [prepare](./prepare.md)

# prepare

`abstract fun prepare(): `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/window/WindowRequest.kt#L27)

Prepares an [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) for the window request. This is used to
attach state (e.g. a native session or view) to the engine session.

**Return**
the prepared and ready-to-use [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md).

