[android-components](../../index.md) / [mozilla.components.support.base.observer](../index.md) / [ObserverRegistry](index.md) / [unregister](./unregister.md)

# unregister

`@Synchronized fun unregister(observer: `[`T`](index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/observer/ObserverRegistry.kt#L92)

Overrides [Observable.unregister](../-observable/unregister.md)

Unregisters an observer. Does nothing if [observer](unregister.md#mozilla.components.support.base.observer.ObserverRegistry$unregister(mozilla.components.support.base.observer.ObserverRegistry.T)/observer) is not registered.

### Parameters

`observer` - the observer to unregister.