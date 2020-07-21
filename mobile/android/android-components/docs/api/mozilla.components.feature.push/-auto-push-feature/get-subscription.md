[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [AutoPushFeature](index.md) / [getSubscription](./get-subscription.md)

# getSubscription

`fun getSubscription(scope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, appServerKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, block: (`[`AutoPushSubscription`](../-auto-push-subscription/index.md)`?) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/AutoPushFeature.kt#L241)

Checks if a subscription for the [scope](get-subscription.md#mozilla.components.feature.push.AutoPushFeature$getSubscription(kotlin.String, kotlin.String, kotlin.Function1((mozilla.components.feature.push.AutoPushSubscription, kotlin.Unit)))/scope) already exists.

### Parameters

`scope` - The subscription identifier which usually represents the website's URI.

`appServerKey` - An optional key provided by the application server.

`block` - The callback invoked when a subscription for the [scope](get-subscription.md#mozilla.components.feature.push.AutoPushFeature$getSubscription(kotlin.String, kotlin.String, kotlin.Function1((mozilla.components.feature.push.AutoPushSubscription, kotlin.Unit)))/scope) is found, otherwise null. Note: this will
not execute on the calls thread.