[android-components](../../index.md) / [mozilla.components.concept.engine.webpush](../index.md) / [WebPushDelegate](index.md) / [onUnsubscribe](./on-unsubscribe.md)

# onUnsubscribe

`open fun onUnsubscribe(scope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, onUnsubscribe: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webpush/WebPushDelegate.kt#L27)

Remove a subscription for the given Service Worker scope.

**Return**
whether the unsubscribe was successful or not.

