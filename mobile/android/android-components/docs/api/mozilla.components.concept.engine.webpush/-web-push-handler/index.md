[android-components](../../index.md) / [mozilla.components.concept.engine.webpush](../index.md) / [WebPushHandler](./index.md)

# WebPushHandler

`interface WebPushHandler` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webpush/WebPush.kt#L14)

A handler for all WebPush messages and [subscriptions](https://developer.mozilla.org/en-US/docs/Web/API/PushSubscription) to be delivered to the [Engine](../../mozilla.components.concept.engine/-engine/index.md).

### Functions

| Name | Summary |
|---|---|
| [onPushMessage](on-push-message.md) | `abstract fun onPushMessage(scope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, message: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked when a push message has been delivered. |
| [onSubscriptionChanged](on-subscription-changed.md) | `open fun onSubscriptionChanged(scope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked when a subscription has now changed/expired. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
