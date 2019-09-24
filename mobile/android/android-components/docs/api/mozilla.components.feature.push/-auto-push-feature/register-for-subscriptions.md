[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [AutoPushFeature](index.md) / [registerForSubscriptions](./register-for-subscriptions.md)

# registerForSubscriptions

`fun registerForSubscriptions(observer: `[`PushSubscriptionObserver`](../-push-subscription-observer/index.md)`, owner: LifecycleOwner = ProcessLifecycleOwner.get(), autoPause: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/AutoPushFeature.kt#L175)

Register to receive push subscriptions when requested or when they have been re-registered.

### Parameters

`observer` - the observer that will be notified.

`owner` - the lifecycle owner for the observer. Defaults to [ProcessLifecycleOwner](#).

`autoPause` - whether to stop notifying the observer during onPause lifecycle events.
Defaults to false so that subscriptions are always delivered to observers.