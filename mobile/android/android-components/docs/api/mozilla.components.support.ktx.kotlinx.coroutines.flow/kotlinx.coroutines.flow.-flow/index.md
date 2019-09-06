[android-components](../../index.md) / [mozilla.components.support.ktx.kotlinx.coroutines.flow](../index.md) / [kotlinx.coroutines.flow.Flow](./index.md)

### Extensions for kotlinx.coroutines.flow.Flow

| Name | Summary |
|---|---|
| [ifChanged](if-changed.md) | `fun <T> Flow<`[`T`](if-changed.md#T)`>.ifChanged(): Flow<`[`T`](if-changed.md#T)`>`<br>Returns a [Flow](#) containing only values of the original [Flow](#) that have changed compared to the value emitted before them.`fun <T, R> Flow<`[`T`](if-changed.md#T)`>.ifChanged(transform: (`[`T`](if-changed.md#T)`) -> `[`R`](if-changed.md#R)`): Flow<`[`T`](if-changed.md#T)`>`<br>Returns a [Flow](#) containing only values of the original [Flow](#) where the result of calling [transform](if-changed.md#mozilla.components.support.ktx.kotlinx.coroutines.flow$ifChanged(kotlinx.coroutines.flow.Flow((mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged.T)), kotlin.Function1((mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged.T, mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged.R)))/transform) has changed from the result of the previous value. |
