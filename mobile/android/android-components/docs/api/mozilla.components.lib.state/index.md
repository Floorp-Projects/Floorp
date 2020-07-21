[android-components](../index.md) / [mozilla.components.lib.state](./index.md)

## Package mozilla.components.lib.state

### Types

| Name | Summary |
|---|---|
| [Action](-action.md) | `interface Action`<br>Generic interface for actions to be dispatched on a [Store](-store/index.md). |
| [MiddlewareStore](-middleware-store/index.md) | `interface MiddlewareStore<S, A>`<br>A simplified [Store](-store/index.md) interface for the purpose of passing it to a [Middleware](-middleware.md). |
| [State](-state.md) | `interface State`<br>Generic interface for a [State](-state.md) maintained by a [Store](-store/index.md). |
| [Store](-store/index.md) | `open class Store<S : `[`State`](-state.md)`, A : `[`Action`](-action.md)`>`<br>A generic store holding an immutable [State](-state.md). |

### Exceptions

| Name | Summary |
|---|---|
| [StoreException](-store-exception/index.md) | `class StoreException : `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)<br>Exception for otherwise unhandled errors caught while reducing state or while managing/notifying observers. |

### Type Aliases

| Name | Summary |
|---|---|
| [Middleware](-middleware.md) | `typealias Middleware<S, A> = (store: `[`MiddlewareStore`](-middleware-store/index.md)`<`[`S`](-middleware.md#S)`, `[`A`](-middleware.md#A)`>, next: (`[`A`](-middleware.md#A)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, action: `[`A`](-middleware.md#A)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A [Middleware](-middleware.md) sits between the store and the reducer. It provides an extension point between dispatching an action, and the moment it reaches the reducer. |
| [Observer](-observer.md) | `typealias Observer<S> = (`[`S`](-observer.md#S)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Listener called when the state changes in the [Store](-store/index.md). |
| [Reducer](-reducer.md) | `typealias Reducer<S, A> = (`[`S`](-reducer.md#S)`, `[`A`](-reducer.md#A)`) -> `[`S`](-reducer.md#S)<br>Reducers specify how the application's [State](-state.md) changes in response to [Action](-action.md)s sent to the [Store](-store/index.md). |
