[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [AutoPushFeature](index.md) / [unsubscribeForType](./unsubscribe-for-type.md)

# unsubscribeForType

`fun unsubscribeForType(type: `[`PushType`](../-push-type/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/AutoPushFeature.kt#L214)

Returns subscription information for the push type if available.

Implementation notes: We need to connect this to the device constellation so that we update our subscriptions
when notified by FxA. See [#3859](https://github.com/mozilla-mobile/android-components/issues/3859).

