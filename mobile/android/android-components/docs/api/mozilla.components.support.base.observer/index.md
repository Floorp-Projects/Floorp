[android-components](../index.md) / [mozilla.components.support.base.observer](./index.md)

## Package mozilla.components.support.base.observer

### Types

| Name | Summary |
|---|---|
| [Consumable](-consumable/index.md) | `class Consumable<T>`<br>A generic wrapper for values that can get consumed. |
| [ConsumableStream](-consumable-stream/index.md) | `class ConsumableStream<T>`<br>A generic wrapper for a stream of values that can be consumed. Values will be consumed first in, first out. |
| [Observable](-observable/index.md) | `interface Observable<T>`<br>Interface for observables. This interface is implemented by ObserverRegistry so that classes that want to be observable can implement the interface by delegation: |
| [ObserverRegistry](-observer-registry/index.md) | `class ObserverRegistry<T> : `[`Observable`](-observable/index.md)`<`[`T`](-observer-registry/index.md#T)`>`<br>A helper for classes that want to get observed. This class keeps track of registered observers and can automatically unregister observers if a LifecycleOwner is provided. |

### Type Aliases

| Name | Summary |
|---|---|
| [ConsumableListener](-consumable-listener.md) | `typealias ConsumableListener = () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
