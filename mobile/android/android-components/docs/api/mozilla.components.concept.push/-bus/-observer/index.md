[android-components](../../../index.md) / [mozilla.components.concept.push](../../index.md) / [Bus](../index.md) / [Observer](./index.md)

# Observer

`interface Observer<T, M>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/push/src/main/java/mozilla/components/concept/push/Bus.kt#L65)

An observer interface that all listeners have to implement in order to register and receive events.

### Functions

| Name | Summary |
|---|---|
| [onEvent](on-event.md) | `abstract fun onEvent(type: `[`T`](index.md#T)`, message: `[`M`](index.md#M)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
