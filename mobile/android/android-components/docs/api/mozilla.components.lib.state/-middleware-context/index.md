[android-components](../../index.md) / [mozilla.components.lib.state](../index.md) / [MiddlewareContext](./index.md)

# MiddlewareContext

`interface MiddlewareContext<S : `[`State`](../-state.md)`, A : `[`Action`](../-action.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/state/src/main/java/mozilla/components/lib/state/Middleware.kt#L28)

The context a Middleware is running in. Allows access to privileged [Store](../-store/index.md) functionality. It is
passed to a [Middleware](../-middleware.md) with every [Action](../-action.md).

Note that the [MiddlewareContext](./index.md) should not be passed to other components and calling methods
on non-[Store](../-store/index.md) threads may throw an exception. Instead the value of the [store](store.md) property, granting
access to the underlying store, can safely be used outside of the middleware.

### Properties

| Name | Summary |
|---|---|
| [state](state.md) | `abstract val state: `[`S`](index.md#S)<br>Returns the current state of the [Store](../-store/index.md). |
| [store](store.md) | `abstract val store: `[`Store`](../-store/index.md)`<`[`S`](index.md#S)`, `[`A`](index.md#A)`>`<br>Returns a reference to the [Store](../-store/index.md) the [Middleware](../-middleware.md) is running in. |

### Functions

| Name | Summary |
|---|---|
| [dispatch](dispatch.md) | `abstract fun dispatch(action: `[`A`](index.md#A)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Dispatches an [Action](../-action.md) synchronously on the [Store](../-store/index.md). Other than calling [Store.dispatch](../-store/dispatch.md), this will block and return after all [Store](../-store/index.md) observers have been notified about the state change. The dispatched [Action](../-action.md) will go through the whole chain of middleware again. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
