[android-components](../../index.md) / [mozilla.components.browser.engine.system.window](../index.md) / [SystemWindowRequest](index.md) / [prepare](./prepare.md)

# prepare

`fun prepare(): `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/window/SystemWindowRequest.kt#L34)

Overrides [WindowRequest.prepare](../../mozilla.components.concept.engine.window/-window-request/prepare.md)

Prepares an [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) for the window request. This is used to
attach state (e.g. a native session or view) to the engine session.

**Return**
the prepared and ready-to-use [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md).

