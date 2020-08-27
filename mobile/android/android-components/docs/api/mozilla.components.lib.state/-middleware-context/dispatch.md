[android-components](../../index.md) / [mozilla.components.lib.state](../index.md) / [MiddlewareContext](index.md) / [dispatch](./dispatch.md)

# dispatch

`abstract fun dispatch(action: `[`A`](index.md#A)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/state/src/main/java/mozilla/components/lib/state/Middleware.kt#L47)

Dispatches an [Action](../-action.md) synchronously on the [Store](../-store/index.md). Other than calling [Store.dispatch](../-store/dispatch.md), this
will block and return after all [Store](../-store/index.md) observers have been notified about the state change.
The dispatched [Action](../-action.md) will go through the whole chain of middleware again.

This method is particular useful if a middleware wants to dispatch an additional [Action](../-action.md) and
wait until the [state](state.md) has been updated to further process it.

Note that this method should only ever be called from a [Middleware](../-middleware.md) and the calling thread.
Calling it from another thread may throw an exception. For dispatching an [Action](../-action.md) from
asynchronous code in the [Middleware](../-middleware.md) or another component use [store](store.md) which returns a
reference to the underlying [Store](../-store/index.md) that offers methods for asynchronous dispatching.

