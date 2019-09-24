[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [MessageBus](./index.md)

# MessageBus

`class MessageBus<T : `[`Enum`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-enum/index.html)`<`[`T`](index.md#T)`>, M> : `[`Bus`](../../mozilla.components.concept.push/-bus/index.md)`<`[`T`](index.md#T)`, `[`M`](index.md#M)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/MessageBus.kt#L16)

An implementation of [Bus](../../mozilla.components.concept.push/-bus/index.md) where the event type is restricted to an enum.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `MessageBus()`<br>An implementation of [Bus](../../mozilla.components.concept.push/-bus/index.md) where the event type is restricted to an enum. |

### Functions

| Name | Summary |
|---|---|
| [notifyObservers](notify-observers.md) | `fun notifyObservers(type: `[`T`](index.md#T)`, message: `[`M`](index.md#M)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies all registered observers of a particular message. |
| [register](register.md) | `fun register(type: `[`T`](index.md#T)`, observer: `[`Observer`](../../mozilla.components.concept.push/-bus/-observer/index.md)`<`[`T`](index.md#T)`, `[`M`](index.md#M)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>`fun register(type: `[`T`](index.md#T)`, observer: `[`Observer`](../../mozilla.components.concept.push/-bus/-observer/index.md)`<`[`T`](index.md#T)`, `[`M`](index.md#M)`>, owner: LifecycleOwner, autoPause: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers an observer to get notified about events. |
| [unregister](unregister.md) | `fun unregister(type: `[`T`](index.md#T)`, observer: `[`Observer`](../../mozilla.components.concept.push/-bus/-observer/index.md)`<`[`T`](index.md#T)`, `[`M`](index.md#M)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Unregisters an observer to stop getting notified about events. |
