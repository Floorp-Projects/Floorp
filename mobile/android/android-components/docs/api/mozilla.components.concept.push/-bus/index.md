[android-components](../../index.md) / [mozilla.components.concept.push](../index.md) / [Bus](./index.md)

# Bus

`interface Bus<T, M>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/push/src/main/java/mozilla/components/concept/push/Bus.kt#L31)

Interface for a basic bus that is implemented by MessageBus so that classes can observe particular event types.

```
    bus.register(Type.A, MyObservableClass())

    // In some other class, we can notify the MyObservableClass.
    bus.notifyObservers(Type.A, Message("Hello!"))
```

### Parameters

`T` - A type that's capable of describing message categories (e.g. an enum).

`M` - A type for holding message contents (e.g. data or sealed class).

### Types

| Name | Summary |
|---|---|
| [Observer](-observer/index.md) | `interface Observer<T, M>`<br>An observer interface that all listeners have to implement in order to register and receive events. |

### Functions

| Name | Summary |
|---|---|
| [notifyObservers](notify-observers.md) | `abstract fun notifyObservers(type: `[`T`](index.md#T)`, message: `[`M`](index.md#M)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies all registered observers of a particular message. |
| [register](register.md) | `abstract fun register(type: `[`T`](index.md#T)`, observer: `[`Observer`](-observer/index.md)`<`[`T`](index.md#T)`, `[`M`](index.md#M)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers an observer to get notified about events. |
| [unregister](unregister.md) | `abstract fun unregister(type: `[`T`](index.md#T)`, observer: `[`Observer`](-observer/index.md)`<`[`T`](index.md#T)`, `[`M`](index.md#M)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Unregisters an observer to stop getting notified about events. |
