[android-components](../../index.md) / [mozilla.components.browser.engine.gecko.window](../index.md) / [GeckoWindowRequest](index.md) / [prepare](./prepare.md)

# prepare

`fun prepare(): `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/window/GeckoWindowRequest.kt#L19)

Overrides [WindowRequest.prepare](../../mozilla.components.concept.engine.window/-window-request/prepare.md)

Prepares an [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) for the window request. This is used to
attach state (e.g. a native session or view) to the engine session.

**Return**
the prepared and ready-to-use [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md).

