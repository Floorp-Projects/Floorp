[android-components](../../index.md) / [mozilla.components.support.base.observer](../index.md) / [ObserverRegistry](index.md) / [resumeObserver](./resume-observer.md)

# resumeObserver

`@Synchronized fun resumeObserver(observer: `[`T`](index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/observer/ObserverRegistry.kt#L129)

Overrides [Observable.resumeObserver](../-observable/resume-observer.md)

Resumes the provided observer. Notifications sent since it
was last paused (see [pauseObserver](../-observable/pause-observer.md)]) are lost and will not be
re-delivered.

### Parameters

`observer` - the observer to resume.