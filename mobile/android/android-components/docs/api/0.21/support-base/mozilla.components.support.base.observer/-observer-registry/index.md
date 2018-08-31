---
title: ObserverRegistry - 
---

[mozilla.components.support.base.observer](../index.html) / [ObserverRegistry](./index.html)

# ObserverRegistry

`class ObserverRegistry<T> : `[`Observable`](../-observable/index.html)`<`[`T`](index.html#T)`>`

A helper for classes that want to get observed. This class keeps track of registered observers
and can automatically unregister observers if a LifecycleOwner is provided.

### Constructors

| [&lt;init&gt;](-init-.html) | `ObserverRegistry()`<br>A helper for classes that want to get observed. This class keeps track of registered observers and can automatically unregister observers if a LifecycleOwner is provided. |

### Functions

| [notifyObservers](notify-observers.html) | `fun notifyObservers(block: `[`T`](index.html#T)`.() -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notify all registered observers about a change. |
| [register](register.html) | `fun register(observer: `[`T`](index.html#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>`fun register(observer: `[`T`](index.html#T)`, owner: LifecycleOwner): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>`fun register(observer: `[`T`](index.html#T)`, view: View): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers an observer to get notified about changes. |
| [unregister](unregister.html) | `fun unregister(observer: `[`T`](index.html#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Unregisters an observer. |
| [unregisterObservers](unregister-observers.html) | `fun unregisterObservers(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Unregisters all observers. |
| [wrapConsumers](wrap-consumers.html) | `fun <V> wrapConsumers(block: `[`T`](index.html#T)`.(`[`V`](wrap-consumers.html#V)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<(`[`V`](wrap-consumers.html#V)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>Returns a list of lambdas wrapping a consuming method of an observer. |

