[android-components](../../index.md) / [mozilla.components.support.base.observer](../index.md) / [ObserverRegistry](index.md) / [notifyObservers](./notify-observers.md)

# notifyObservers

`@Synchronized fun notifyObservers(block: `[`T`](index.md#T)`.() -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/observer/ObserverRegistry.kt#L134)

Overrides [Observable.notifyObservers](../-observable/notify-observers.md)

Notifies all registered observers about a change.

### Parameters

`block` - the notification (method on the observer to be invoked).