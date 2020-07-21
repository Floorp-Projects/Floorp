[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [Engine](index.md) / [registerWebPushDelegate](./register-web-push-delegate.md)

# registerWebPushDelegate

`open fun registerWebPushDelegate(webPushDelegate: `[`WebPushDelegate`](../../mozilla.components.concept.engine.webpush/-web-push-delegate/index.md)`): `[`WebPushHandler`](../../mozilla.components.concept.engine.webpush/-web-push-handler/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/Engine.kt#L154)

Registers a [WebPushDelegate](../../mozilla.components.concept.engine.webpush/-web-push-delegate/index.md) to be notified of engine events related to web extensions.

**Return**
A [WebPushHandler](../../mozilla.components.concept.engine.webpush/-web-push-handler/index.md) to notify the engine with messages and subscriptions when are delivered.

