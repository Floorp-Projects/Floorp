[android-components](../../index.md) / [mozilla.components.support.ktx.kotlinx.coroutines.flow](../index.md) / [kotlinx.coroutines.flow.Flow](index.md) / [ifAnyChanged](./if-any-changed.md)

# ifAnyChanged

`fun <T, R> Flow<`[`T`](if-any-changed.md#T)`>.ifAnyChanged(transform: (`[`T`](if-any-changed.md#T)`) -> `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`R`](if-any-changed.md#R)`>): Flow<`[`T`](if-any-changed.md#T)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/kotlinx/coroutines/flow/Flow.kt#L119)

Returns a [Flow](#) containing only values of the original [Flow](#) where the result array
of calling [transform](if-any-changed.md#mozilla.components.support.ktx.kotlinx.coroutines.flow$ifAnyChanged(kotlinx.coroutines.flow.Flow((mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged.T)), kotlin.Function1((mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged.T, kotlin.Array((mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged.R)))))/transform) contains at least one different value.

Example:

```
Block: x -> [x[0], x[1]]  // Map to first two characters of input
Original Flow: "banana", "bandanna", "bus", "apple", "big", "coconut", "circle", "home"
Mapped: [b, a], [b, a], [b, u], [a, p], [b, i], [c, o], [c, i], [h, o]
Returned Flow: "banana", "bus, "apple", "big", "coconut", "circle", "home"
``
```

