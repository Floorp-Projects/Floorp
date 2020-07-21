[android-components](../../index.md) / [mozilla.components.support.base.observer](../index.md) / [Observable](index.md) / [notifyAtLeastOneObserver](./notify-at-least-one-observer.md)

# notifyAtLeastOneObserver

`abstract fun notifyAtLeastOneObserver(block: `[`T`](index.md#T)`.() -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/observer/Observable.kt#L81)

Notifies all registered observers about a change. If there is no observer
the notification is queued and sent to the first observer that is
registered.

### Parameters

`block` - the notification (method on the observer to be invoked).