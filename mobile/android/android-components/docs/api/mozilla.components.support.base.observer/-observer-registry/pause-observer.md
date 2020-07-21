[android-components](../../index.md) / [mozilla.components.support.base.observer](../index.md) / [ObserverRegistry](index.md) / [pauseObserver](./pause-observer.md)

# pauseObserver

`@Synchronized fun pauseObserver(observer: `[`T`](index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/observer/ObserverRegistry.kt#L124)

Overrides [Observable.pauseObserver](../-observable/pause-observer.md)

Pauses the provided observer. No notifications will be sent to this
observer until [resumeObserver](../-observable/resume-observer.md) is called.

### Parameters

`observer` - the observer to pause.