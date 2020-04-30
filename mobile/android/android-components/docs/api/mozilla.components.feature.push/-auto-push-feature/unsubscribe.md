[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [AutoPushFeature](index.md) / [unsubscribe](./unsubscribe.md)

# unsubscribe

`fun unsubscribe(scope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, onUnsubscribeError: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {}, onUnsubscribe: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {}): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/AutoPushFeature.kt#L213)

Un-subscribes from a valid subscription and invokes the [onUnsubscribe](unsubscribe.md#mozilla.components.feature.push.AutoPushFeature$unsubscribe(kotlin.String, kotlin.Function0((kotlin.Unit)), kotlin.Function1((kotlin.Boolean, kotlin.Unit)))/onUnsubscribe) callback with the result.

### Parameters

`scope` - The subscription identifier which usually represents the website's URI.

`onUnsubscribeError` - The callback invoked if the call does not successfully complete.

`onUnsubscribe` - The callback invoked when a subscription for the [scope](unsubscribe.md#mozilla.components.feature.push.AutoPushFeature$unsubscribe(kotlin.String, kotlin.Function0((kotlin.Unit)), kotlin.Function1((kotlin.Boolean, kotlin.Unit)))/scope) is removed.