---
title: Consumable.consumeBy - 
---

[mozilla.components.support.base.observer](../index.html) / [Consumable](index.html) / [consumeBy](./consume-by.html)

# consumeBy

`@Synchronized fun consumeBy(consumers: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<(`[`T`](index.html#T)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)

Invokes the given list of lambdas and marks the value as consumed if at least one lambda
returned true.

