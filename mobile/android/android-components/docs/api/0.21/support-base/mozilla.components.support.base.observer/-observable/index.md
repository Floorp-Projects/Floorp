---
title: Observable - 
---

[mozilla.components.support.base.observer](../index.html) / [Observable](./index.html)

# Observable

`interface Observable<T>`

Interface for observables. This interface is implemented by ObserverRegistry so that classes that
want to be observable can implement the interface by delegation:

```
    class MyObservableClass : Observable<MyObserverInterface> by registry {
      ...
    }
```

### Functions

| [notifyObservers](notify-observers.html) | `abstract fun notifyObservers(block: `[`T`](index.html#T)`.() -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notify all registered observers about a change. |
| [register](register.html) | `abstract fun register(observer: `[`T`](index.html#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>`abstract fun register(observer: `[`T`](index.html#T)`, owner: LifecycleOwner): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>`abstract fun register(observer: `[`T`](index.html#T)`, view: View): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers an observer to get notified about changes. |
| [unregister](unregister.html) | `abstract fun unregister(observer: `[`T`](index.html#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Unregisters an observer. |
| [unregisterObservers](unregister-observers.html) | `abstract fun unregisterObservers(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Unregisters all observers. |
| [wrapConsumers](wrap-consumers.html) | `abstract fun <R> wrapConsumers(block: `[`T`](index.html#T)`.(`[`R`](wrap-consumers.html#R)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<(`[`R`](wrap-consumers.html#R)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>Returns a list of lambdas wrapping a consuming method of an observer. |

### Inheritors

| [ObserverRegistry](../-observer-registry/index.html) | `class ObserverRegistry<T> : `[`Observable`](./index.md)`<`[`T`](../-observer-registry/index.html#T)`>`<br>A helper for classes that want to get observed. This class keeps track of registered observers and can automatically unregister observers if a LifecycleOwner is provided. |

