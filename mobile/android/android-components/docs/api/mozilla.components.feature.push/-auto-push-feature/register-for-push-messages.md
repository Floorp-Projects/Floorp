[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [AutoPushFeature](index.md) / [registerForPushMessages](./register-for-push-messages.md)

# registerForPushMessages

`fun registerForPushMessages(type: `[`PushType`](../-push-type/index.md)`, observer: `[`Observer`](../../mozilla.components.concept.push/-bus/-observer/index.md)`<`[`PushType`](../-push-type/index.md)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, owner: LifecycleOwner = ProcessLifecycleOwner.get(), autoPause: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/AutoPushFeature.kt#L190)

Register to receive push messages for the associated [PushType](../-push-type/index.md).

### Parameters

`type` - the push message type that you want to be registered.

`observer` - the observer that will be notified.

`owner` - the lifecycle owner for the observer. Defaults to [ProcessLifecycleOwner](#).

`autoPause` - whether to stop notifying the observer during onPause lifecycle events.
Defaults to false so that messages are always delivered to observers.