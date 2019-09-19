[android-components](../../index.md) / [mozilla.components.support.base.observer](../index.md) / [ConsumableStream](./index.md)

# ConsumableStream

`class ConsumableStream<T>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/observer/Consumable.kt#L104)

A generic wrapper for a stream of values that can be consumed. Values will
be consumed first in, first out.

### Functions

| Name | Summary |
|---|---|
| [append](append.md) | `fun append(vararg values: `[`T`](index.md#T)`): `[`ConsumableStream`](./index.md)`<`[`T`](index.md#T)`>`<br>Copies the stream and appends the provided values. |
| [consumeAll](consume-all.md) | `fun consumeAll(consumer: (value: `[`T`](index.md#T)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Invokes the given lambda for each consumable value and marks the values as consumed if the lambda returns true. |
| [consumeAllBy](consume-all-by.md) | `fun consumeAllBy(consumers: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<(`[`T`](index.md#T)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Invokes the given list of lambdas for each consumable value and marks the values as consumed if at least one lambda returns true. |
| [consumeNext](consume-next.md) | `fun consumeNext(consumer: (value: `[`T`](index.md#T)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Invokes the given lambda with the next consumable value and marks the value as consumed if the lambda returns true. |
| [consumeNextBy](consume-next-by.md) | `fun consumeNextBy(consumers: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<(`[`T`](index.md#T)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Invokes the given list of lambdas with the next consumable value and marks the value as consumed if at least one lambda returns true. |
| [isConsumed](is-consumed.md) | `fun isConsumed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if all values in this stream were consumed, otherwise false. |
| [isEmpty](is-empty.md) | `fun isEmpty(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if the stream is empty, otherwise false. |
| [remove](remove.md) | `fun remove(value: `[`T`](index.md#T)`): `[`ConsumableStream`](./index.md)`<`[`T`](index.md#T)`>`<br>Copies the stream but removes all consumables equal to the provided value. |
| [removeConsumed](remove-consumed.md) | `fun removeConsumed(): `[`ConsumableStream`](./index.md)`<`[`T`](index.md#T)`>`<br>Copies the stream but removes all consumed values. |
