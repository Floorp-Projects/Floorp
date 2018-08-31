---
title: FxaResult.then - 
---

[mozilla.components.service.fxa](../index.html) / [FxaResult](index.html) / [then](./then.html)

# then

`fun <U> then(fn: (value: `[`T`](index.html#T)`) -> `[`FxaResult`](index.html)`<`[`U`](then.html#U)`>?): `[`FxaResult`](index.html)`<`[`U`](then.html#U)`>`

Adds a value listener to be called when the [FxaResult](index.html) is completed with
a value. Listeners will be invoked on the same thread in which the
[FxaResult](index.html) was completed.

### Parameters

`fn` - A lambda expression with the same method signature as [OnValueListener](-on-value-listener/index.html),
called when the [FxaResult](index.html) is completed with a value.`fun <U> then(vfn: (value: `[`T`](index.html#T)`) -> `[`FxaResult`](index.html)`<`[`U`](then.html#U)`>?, efn: (`[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)`) -> `[`FxaResult`](index.html)`<`[`U`](then.html#U)`>?): `[`FxaResult`](index.html)`<`[`U`](then.html#U)`>`

Adds listeners to be called when the [FxaResult](index.html) is completed either with
a value or [Exception](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html). Listeners will be invoked on the same thread in which the
[FxaResult](index.html) was completed.

### Parameters

`vfn` - A lambda expression with the same method signature as [OnValueListener](-on-value-listener/index.html),
called when the [FxaResult](index.html) is completed with a value.

`efn` - A lambda expression with the same method signature as [OnExceptionListener](-on-exception-listener/index.html),
called when the [FxaResult](index.html) is completed with an exception.`@Synchronized fun <U> then(valueListener: `[`OnValueListener`](-on-value-listener/index.html)`<`[`T`](index.html#T)`, `[`U`](then.html#U)`>, exceptionListener: `[`OnExceptionListener`](-on-exception-listener/index.html)`<`[`U`](then.html#U)`>?): `[`FxaResult`](index.html)`<`[`U`](then.html#U)`>`

Adds listeners to be called when the [FxaResult](index.html) is completed either with
a value or [Exception](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html). Listeners will be invoked on the same thread in which the
[FxaResult](index.html) was completed.

### Parameters

`valueListener` - An instance of [OnValueListener](-on-value-listener/index.html), called when the
[FxaResult](index.html) is completed with a value.

`exceptionListener` - An instance of [OnExceptionListener](-on-exception-listener/index.html), called when the
[FxaResult](index.html) is completed with an [Exception](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html).