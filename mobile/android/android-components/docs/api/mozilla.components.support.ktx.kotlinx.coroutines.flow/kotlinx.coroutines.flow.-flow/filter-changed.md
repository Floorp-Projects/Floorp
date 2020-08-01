[android-components](../../index.md) / [mozilla.components.support.ktx.kotlinx.coroutines.flow](../index.md) / [kotlinx.coroutines.flow.Flow](index.md) / [filterChanged](./filter-changed.md)

# filterChanged

`fun <T, R> Flow<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`T`](filter-changed.md#T)`>>.filterChanged(transform: (`[`T`](filter-changed.md#T)`) -> `[`R`](filter-changed.md#R)`): Flow<`[`T`](filter-changed.md#T)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/kotlinx/coroutines/flow/Flow.kt#L91)

Returns a [Flow](#) containing only changed elements of the lists of the original [Flow](#).

```
Example: Identity function
Transform: x -> x (transformed values are the same as original)
Original Flow: list(0), list(0, 1), list(0, 1, 2, 3), list(4), list(5, 6, 7, 8)
Transformed:
(0)          -> (0 emitted because it is a new value)

(0, 1)       -> (0 not emitted because same as previous value,
                 1 emitted because it is a new value),

(0, 1, 2, 3) -> (0 and 1 not emitted because same as previous values,
                 2 and 3 emitted because they are new values),

(4)          -> (4 emitted because because it is a new value)

(5, 6, 7, 8) -> (5, 6, 7, 8 emitted because they are all new values)
Returned Flow: 0, 1, 2, 3, 4, 5, 6, 7, 8
---

Example: Modulo 2
Transform: x -> x % 2 (emit changed values if the result of modulo 2 changed)
Original Flow: listOf(1), listOf(1, 2), listOf(3, 4, 5), listOf(3, 4)
Transformed:
(1)          -> (1 emitted because it is a new value)

(1, 0)       -> (1 not emitted because same as previous value with the same transformed value,
                 2 emitted because it is a new value),

(1, 0, 1)    -> (3, 4, 5 emitted because they are all new values)

(1, 0)       -> (3, 4 not emitted because same as previous values with same transformed values)

Returned Flow: 1, 2, 3, 4, 5
---
```

