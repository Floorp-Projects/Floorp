---
title: FxaResult.completeExceptionally - 
---

[mozilla.components.service.fxa](../index.html) / [FxaResult](index.html) / [completeExceptionally](./complete-exceptionally.html)

# completeExceptionally

`@Synchronized fun completeExceptionally(exception: `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)

This completes the result with the specified [Exception](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html). IllegalStateException is thrown
if the result is already complete.

### Parameters

`exception` - The [Exception](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html) used to complete the result.

### Exceptions

`IllegalStateException` - 