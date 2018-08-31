---
title: FxaResult.OnValueListener - 
---

[mozilla.components.service.fxa](../../index.html) / [FxaResult](../index.html) / [OnValueListener](./index.html)

# OnValueListener

`@FunctionalInterface interface OnValueListener<T, U>`

An interface used to deliver values to listeners of a [FxaResult](../index.html)

**Parameters**

**Parameters**

### Functions

| [onValue](on-value.html) | `abstract fun onValue(value: `[`T`](index.html#T)`): `[`FxaResult`](../index.html)`<`[`U`](index.html#U)`>?`<br>Called when a [FxaResult](../index.html) is completed with a value. This will be called on the same thread in which the result was completed. |

