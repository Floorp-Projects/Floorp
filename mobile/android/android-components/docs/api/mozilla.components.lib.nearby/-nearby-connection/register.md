[android-components](../../index.md) / [mozilla.components.lib.nearby](../index.md) / [NearbyConnection](index.md) / [register](./register.md)

# register

`fun register(observer: `[`NearbyConnectionObserver`](../-nearby-connection-observer/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/nearby/src/main/java/mozilla/components/lib/nearby/NearbyConnection.kt#L232)

Overrides [Observable.register](../../mozilla.components.support.base.observer/-observable/register.md)

Registers an observer to get notified about changes.

### Parameters

`observer` - the observer to register.`fun register(observer: `[`NearbyConnectionObserver`](../-nearby-connection-observer/index.md)`, view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/nearby/src/main/java/mozilla/components/lib/nearby/NearbyConnection.kt#L237)

Overrides [Observable.register](../../mozilla.components.support.base.observer/-observable/register.md)

Registers an observer to get notified about changes.

The observer will only be notified if the view is attached and will be unregistered/
registered if the attached state changes.

### Parameters

`observer` - the observer to register.

`view` - the view the provided observer is bound to.`fun register(observer: `[`NearbyConnectionObserver`](../-nearby-connection-observer/index.md)`, owner: LifecycleOwner, autoPause: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/nearby/src/main/java/mozilla/components/lib/nearby/NearbyConnection.kt#L242)

Overrides [Observable.register](../../mozilla.components.support.base.observer/-observable/register.md)

Registers an observer to get notified about changes.

The observer will automatically unsubscribe if the lifecycle of the provided LifecycleOwner
becomes DESTROYED.

### Parameters

`observer` - the observer to register.

`owner` - the lifecycle owner the provided observer is bound to.

`autoPause` - whether or not the observer should automatically be
paused/resumed with the bound lifecycle.