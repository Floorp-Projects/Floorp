---
title: FxaResult.whenComplete - 
---

[mozilla.components.service.fxa](../index.html) / [FxaResult](index.html) / [whenComplete](./when-complete.html)

# whenComplete

`fun whenComplete(fn: (value: `[`T`](index.html#T)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)

Adds a value listener to be called when the [FxaResult](index.html) and the whole chain of [then](then.html)
calls is completed with a value. Listeners will be invoked on the same thread in
which the [FxaResult](index.html) was completed.

### Parameters

`fn` - A lambda expression with the same method signature as [OnValueListener](-on-value-listener/index.html),
called when the [FxaResult](index.html) is completed with a value.