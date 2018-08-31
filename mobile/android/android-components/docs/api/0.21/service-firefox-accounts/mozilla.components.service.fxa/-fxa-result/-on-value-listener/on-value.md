---
title: FxaResult.OnValueListener.onValue - 
---

[mozilla.components.service.fxa](../../index.html) / [FxaResult](../index.html) / [OnValueListener](index.html) / [onValue](./on-value.html)

# onValue

`abstract fun onValue(value: `[`T`](index.html#T)`): `[`FxaResult`](../index.html)`<`[`U`](index.html#U)`>?`

Called when a [FxaResult](../index.html) is completed with a value. This will be
called on the same thread in which the result was completed.

### Parameters

`value` - The value of the [FxaResult](../index.html)

**Return**
A new [FxaResult](../index.html), used for chaining results together.
May be null.

