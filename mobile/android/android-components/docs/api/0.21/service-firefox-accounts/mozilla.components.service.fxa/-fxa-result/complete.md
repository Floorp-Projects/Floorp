---
title: FxaResult.complete - 
---

[mozilla.components.service.fxa](../index.html) / [FxaResult](index.html) / [complete](./complete.html)

# complete

`@Synchronized fun complete(value: `[`T`](index.html#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)

This completes the result with the specified value. IllegalStateException is thrown
if the result is already complete.

### Parameters

`value` - The value used to complete the result.

### Exceptions

`IllegalStateException` - 