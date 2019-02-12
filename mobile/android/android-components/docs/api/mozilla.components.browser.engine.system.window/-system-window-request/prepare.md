[android-components](../../index.md) / [mozilla.components.browser.engine.system.window](../index.md) / [SystemWindowRequest](index.md) / [prepare](./prepare.md)

# prepare

`fun prepare(engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/window/SystemWindowRequest.kt#L32)

Overrides [WindowRequest.prepare](../../mozilla.components.concept.engine.window/-window-request/prepare.md)

Prepares the provided [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) for the window request. This
is used to attach state (e.g. a native session) to the engine session.

### Parameters

`engineSession` - the engine session to prepare.