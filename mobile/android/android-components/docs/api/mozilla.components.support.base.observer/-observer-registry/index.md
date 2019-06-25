[android-components](../../index.md) / [mozilla.components.support.base.observer](../index.md) / [ObserverRegistry](./index.md)

# ObserverRegistry

`class ObserverRegistry<T> : `[`Observable`](../-observable/index.md)`<`[`T`](index.md#T)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/observer/ObserverRegistry.kt#L27)

A helper for classes that want to get observed. This class keeps track of registered observers
and can automatically unregister observers if a LifecycleOwner is provided.

ObserverRegistry is thread-safe.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ObserverRegistry()`<br>A helper for classes that want to get observed. This class keeps track of registered observers and can automatically unregister observers if a LifecycleOwner is provided. |

### Functions

| Name | Summary |
|---|---|
| [isObserved](is-observed.md) | `fun isObserved(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>If the observable has registered observers. |
| [notifyObservers](notify-observers.md) | `fun notifyObservers(block: `[`T`](index.md#T)`.() -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies all registered observers about a change. |
| [pauseObserver](pause-observer.md) | `fun pauseObserver(observer: `[`T`](index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Pauses the provided observer. No notifications will be sent to this observer until [resumeObserver](../-observable/resume-observer.md) is called. |
| [register](register.md) | `fun register(observer: `[`T`](index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers an observer to get notified about changes. Does nothing if [observer](register.md#mozilla.components.support.base.observer.ObserverRegistry$register(mozilla.components.support.base.observer.ObserverRegistry.T)/observer) is already registered. This method is thread-safe.`fun register(observer: `[`T`](index.md#T)`, owner: LifecycleOwner, autoPause: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>`fun register(observer: `[`T`](index.md#T)`, view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers an observer to get notified about changes. |
| [resumeObserver](resume-observer.md) | `fun resumeObserver(observer: `[`T`](index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Resumes the provided observer. Notifications sent since it was last paused (see [pauseObserver](../-observable/pause-observer.md)]) are lost and will not be re-delivered. |
| [unregister](unregister.md) | `fun unregister(observer: `[`T`](index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Unregisters an observer. Does nothing if [observer](unregister.md#mozilla.components.support.base.observer.ObserverRegistry$unregister(mozilla.components.support.base.observer.ObserverRegistry.T)/observer) is not registered. |
| [unregisterObservers](unregister-observers.md) | `fun unregisterObservers(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Unregisters all observers. |
| [wrapConsumers](wrap-consumers.md) | `fun <V> wrapConsumers(block: `[`T`](index.md#T)`.(`[`V`](wrap-consumers.md#V)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<(`[`V`](wrap-consumers.md#V)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>Returns a list of lambdas wrapping a consuming method of an observer. |
