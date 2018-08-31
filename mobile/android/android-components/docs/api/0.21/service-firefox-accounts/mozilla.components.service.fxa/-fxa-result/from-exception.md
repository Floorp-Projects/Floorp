---
title: FxaResult.fromException - 
---

[mozilla.components.service.fxa](../index.html) / [FxaResult](index.html) / [fromException](./from-exception.html)

# fromException

`fun <T> fromException(exception: `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)`): `[`FxaResult`](index.html)`<`[`T`](from-exception.html#T)`>`

This constructs a result that is completed with the specified [Exception](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html).
May not be null.

### Parameters

`exception` - The exception used to complete the newly created result.

**Return**
The completed [FxaResult](index.html)

