---
title: FxaResult - 
---

[mozilla.components.service.fxa](../index.html) / [FxaResult](./index.html)

# FxaResult

`class FxaResult<T>`

FxaResult is a class that represents an asynchronous result.

**Parameters**

### Types

| [OnExceptionListener](-on-exception-listener/index.html) | `interface OnExceptionListener<V>`<br>An interface used to deliver exceptions to listeners of a [FxaResult](./index.md) |
| [OnValueListener](-on-value-listener/index.html) | `interface OnValueListener<T, U>`<br>An interface used to deliver values to listeners of a [FxaResult](./index.md) |

### Constructors

| [&lt;init&gt;](-init-.html) | `FxaResult()`<br>FxaResult is a class that represents an asynchronous result. |

### Functions

| [complete](complete.html) | `fun complete(value: `[`T`](index.html#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>This completes the result with the specified value. IllegalStateException is thrown if the result is already complete. |
| [completeExceptionally](complete-exceptionally.html) | `fun completeExceptionally(exception: `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>This completes the result with the specified [Exception](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html). IllegalStateException is thrown if the result is already complete. |
| [then](then.html) | `fun <U> then(fn: (value: `[`T`](index.html#T)`) -> `[`FxaResult`](./index.md)`<`[`U`](then.html#U)`>?): `[`FxaResult`](./index.md)`<`[`U`](then.html#U)`>`<br>Adds a value listener to be called when the [FxaResult](./index.md) is completed with a value. Listeners will be invoked on the same thread in which the [FxaResult](./index.md) was completed.`fun <U> then(vfn: (value: `[`T`](index.html#T)`) -> `[`FxaResult`](./index.md)`<`[`U`](then.html#U)`>?, efn: (`[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)`) -> `[`FxaResult`](./index.md)`<`[`U`](then.html#U)`>?): `[`FxaResult`](./index.md)`<`[`U`](then.html#U)`>`<br>`fun <U> then(valueListener: `[`OnValueListener`](-on-value-listener/index.html)`<`[`T`](index.html#T)`, `[`U`](then.html#U)`>, exceptionListener: `[`OnExceptionListener`](-on-exception-listener/index.html)`<`[`U`](then.html#U)`>?): `[`FxaResult`](./index.md)`<`[`U`](then.html#U)`>`<br>Adds listeners to be called when the [FxaResult](./index.md) is completed either with a value or [Exception](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html). Listeners will be invoked on the same thread in which the [FxaResult](./index.md) was completed. |
| [whenComplete](when-complete.html) | `fun whenComplete(fn: (value: `[`T`](index.html#T)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds a value listener to be called when the [FxaResult](./index.md) and the whole chain of [then](then.html) calls is completed with a value. Listeners will be invoked on the same thread in which the [FxaResult](./index.md) was completed. |

### Companion Object Functions

| [fromException](from-exception.html) | `fun <T> fromException(exception: `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)`): `[`FxaResult`](./index.md)`<`[`T`](from-exception.html#T)`>`<br>This constructs a result that is completed with the specified [Exception](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html). May not be null. |
| [fromValue](from-value.html) | `fun <U> fromValue(value: `[`U`](from-value.html#U)`): `[`FxaResult`](./index.md)`<`[`U`](from-value.html#U)`>`<br>This constructs a result that is fulfilled with the specified value. |

