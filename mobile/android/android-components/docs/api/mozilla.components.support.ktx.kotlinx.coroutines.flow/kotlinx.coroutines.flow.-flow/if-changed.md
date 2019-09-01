[android-components](../../index.md) / [mozilla.components.support.ktx.kotlinx.coroutines.flow](../index.md) / [kotlinx.coroutines.flow.Flow](index.md) / [ifChanged](./if-changed.md)

# ifChanged

`fun <T> Flow<`[`T`](if-changed.md#T)`>.ifChanged(): Flow<`[`T`](if-changed.md#T)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/kotlinx/coroutines/flow/Flow.kt#L18)

Returns a [Flow](#) containing only values of the original [Flow](#) that have changed compared to
the value emitted before them.

Example:
Original Flow: A, B, B, C, A, A, A, D, A
Returned Flow: A, B, C, A, D, A

