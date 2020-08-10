[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [AutoPushFeature](index.md) / [subscribe](./subscribe.md)

# subscribe

`fun subscribe(scope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, appServerKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, onSubscribeError: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {}, onSubscribe: (`[`AutoPushSubscription`](../-auto-push-subscription/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {}): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/AutoPushFeature.kt#L190)

Subscribes for push notifications and invokes the [onSubscribe](subscribe.md#mozilla.components.feature.push.AutoPushFeature$subscribe(kotlin.String, kotlin.String, kotlin.Function0((kotlin.Unit)), kotlin.Function1((mozilla.components.feature.push.AutoPushSubscription, kotlin.Unit)))/onSubscribe) callback with the subscription information.

### Parameters

`scope` - The subscription identifier which usually represents the website's URI.

`appServerKey` - An optional key provided by the application server.

`onSubscribeError` - The callback invoked if the call does not successfully complete.

`onSubscribe` - The callback invoked when a subscription for the [scope](subscribe.md#mozilla.components.feature.push.AutoPushFeature$subscribe(kotlin.String, kotlin.String, kotlin.Function0((kotlin.Unit)), kotlin.Function1((mozilla.components.feature.push.AutoPushSubscription, kotlin.Unit)))/scope) is created.