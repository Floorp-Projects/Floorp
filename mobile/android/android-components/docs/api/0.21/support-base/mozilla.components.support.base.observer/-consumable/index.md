---
title: Consumable - 
---

[mozilla.components.support.base.observer](../index.html) / [Consumable](./index.html)

# Consumable

`class Consumable<T>`

A generic wrapper for values that can get consumed.

### Functions

| [consume](consume.html) | `fun consume(consumer: (value: `[`T`](index.html#T)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Invokes the given lambda and marks the value as consumed if the lambda returns true. |
| [consumeBy](consume-by.html) | `fun consumeBy(consumers: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<(`[`T`](index.html#T)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Invokes the given list of lambdas and marks the value as consumed if at least one lambda returned true. |
| [isConsumed](is-consumed.html) | `fun isConsumed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns whether the value was consumed. |

### Companion Object Functions

| [empty](empty.html) | `fun <T> empty(): `[`Consumable`](./index.md)`<`[`T`](empty.html#T)`>`<br>Returns an empty Consumable with not value as if it was consumed already. |
| [from](from.html) | `fun <T> from(value: `[`T`](from.html#T)`): `[`Consumable`](./index.md)`<`[`T`](from.html#T)`>`<br>Create a new Consumable wrapping the given value. |

