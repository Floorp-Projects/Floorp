[android-components](../../index.md) / [mozilla.components.lib.state](../index.md) / [MiddlewareStore](./index.md)

# MiddlewareStore

`interface MiddlewareStore<S, A>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/state/src/main/java/mozilla/components/lib/state/Middleware.kt#L23)

A simplified [Store](../-store/index.md) interface for the purpose of passing it to a [Middleware](../-middleware.md).

### Properties

| Name | Summary |
|---|---|
| [state](state.md) | `abstract val state: `[`S`](index.md#S)<br>Returns the current state of the [Store](../-store/index.md). |

### Functions

| Name | Summary |
|---|---|
| [dispatch](dispatch.md) | `abstract fun dispatch(action: `[`A`](index.md#A)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Dispatches an [Action](../-action.md) synchronously on the [Store](../-store/index.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
