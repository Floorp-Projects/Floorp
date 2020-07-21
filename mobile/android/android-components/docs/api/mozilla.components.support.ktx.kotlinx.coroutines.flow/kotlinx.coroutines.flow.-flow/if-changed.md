[android-components](../../index.md) / [mozilla.components.support.ktx.kotlinx.coroutines.flow](../index.md) / [kotlinx.coroutines.flow.Flow](index.md) / [ifChanged](./if-changed.md)

# ifChanged

`fun <T> Flow<`[`T`](if-changed.md#T)`>.ifChanged(): Flow<`[`T`](if-changed.md#T)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/kotlinx/coroutines/flow/Flow.kt#L22)

Returns a [Flow](#) containing only values of the original [Flow](#) that have changed compared to
the value emitted before them.

Example:

```
Original Flow: A, B, B, C, A, A, A, D, A
Returned Flow: A, B, C, A, D, A
```

`fun <T, R> Flow<`[`T`](if-changed.md#T)`>.ifChanged(transform: (`[`T`](if-changed.md#T)`) -> `[`R`](if-changed.md#R)`): Flow<`[`T`](if-changed.md#T)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/kotlinx/coroutines/flow/Flow.kt#L36)

Returns a [Flow](#) containing only values of the original [Flow](#) where the result of calling
[transform](if-changed.md#mozilla.components.support.ktx.kotlinx.coroutines.flow$ifChanged(kotlinx.coroutines.flow.Flow((mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged.T)), kotlin.Function1((mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged.T, mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged.R)))/transform) has changed from the result of the previous value.

Example:

```
Block: x -> x[0]  // Map to first character of input
Original Flow: "banana", "bus", "apple", "big", "coconut", "circle", "home"
Mapped: b, b, a, b, c, c, h
Returned Flow: "banana", "apple", "big", "coconut", "home"
```

