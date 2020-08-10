[android-components](../../index.md) / [mozilla.components.concept.engine.webpush](../index.md) / [WebPushDelegate](index.md) / [onSubscribe](./on-subscribe.md)

# onSubscribe

`open fun onSubscribe(scope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, serverKey: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`?, onSubscribe: (`[`WebPushSubscription`](../-web-push-subscription/index.md)`?) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webpush/WebPushDelegate.kt#L20)

Create a WebPush subscription for the given Service Worker scope.

