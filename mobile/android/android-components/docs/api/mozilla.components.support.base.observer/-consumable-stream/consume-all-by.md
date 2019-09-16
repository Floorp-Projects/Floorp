[android-components](../../index.md) / [mozilla.components.support.base.observer](../index.md) / [ConsumableStream](index.md) / [consumeAllBy](./consume-all-by.md)

# consumeAllBy

`@Synchronized fun consumeAllBy(consumers: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<(`[`T`](index.md#T)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/observer/Consumable.kt#L153)

Invokes the given list of lambdas for each consumable value and marks the
values as consumed if at least one lambda returns true.

### Parameters

`consumers` - the lambdas accepting a consumable value.

**Return**
true if all consumables were consumed, otherwise false.

