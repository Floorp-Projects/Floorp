[android-components](../../index.md) / [mozilla.components.concept.engine.window](../index.md) / [WindowRequest](index.md) / [prepare](./prepare.md)

# prepare

`abstract fun prepare(engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/window/WindowRequest.kt#L27)

Prepares the provided [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) for the window request. This
is used to attach state (e.g. a native session) to the engine session.

### Parameters

`engineSession` - the engine session to prepare.