[android-components](../../index.md) / [mozilla.components.support.base.observer](../index.md) / [ObserverRegistry](index.md) / [register](./register.md)

# register

`@Synchronized fun register(observer: `[`T`](index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/observer/ObserverRegistry.kt#L40)

Overrides [Observable.register](../-observable/register.md)

Registers an observer to get notified about changes. Does nothing if [observer](register.md#mozilla.components.support.base.observer.ObserverRegistry$register(mozilla.components.support.base.observer.ObserverRegistry.T)/observer) is already registered.
This method is thread-safe.

### Parameters

`observer` - the observer to register.`@Synchronized fun register(observer: `[`T`](index.md#T)`, owner: LifecycleOwner, autoPause: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/observer/ObserverRegistry.kt#L45)

Overrides [Observable.register](../-observable/register.md)

Registers an observer to get notified about changes.

The observer will automatically unsubscribe if the lifecycle of the provided LifecycleOwner
becomes DESTROYED.

### Parameters

`observer` - the observer to register.

`owner` - the lifecycle owner the provided observer is bound to.

`autoPause` - whether or not the observer should automatically be
paused/resumed with the bound lifecycle.`@Synchronized fun register(observer: `[`T`](index.md#T)`, view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/observer/ObserverRegistry.kt#L65)

Overrides [Observable.register](../-observable/register.md)

Registers an observer to get notified about changes.

The observer will only be notified if the view is attached and will be unregistered/
registered if the attached state changes.

### Parameters

`observer` - the observer to register.

`view` - the view the provided observer is bound to.