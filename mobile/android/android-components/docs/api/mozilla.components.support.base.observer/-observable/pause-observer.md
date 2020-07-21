[android-components](../../index.md) / [mozilla.components.support.base.observer](../index.md) / [Observable](index.md) / [pauseObserver](./pause-observer.md)

# pauseObserver

`abstract fun pauseObserver(observer: `[`T`](index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/observer/Observable.kt#L89)

Pauses the provided observer. No notifications will be sent to this
observer until [resumeObserver](resume-observer.md) is called.

### Parameters

`observer` - the observer to pause.