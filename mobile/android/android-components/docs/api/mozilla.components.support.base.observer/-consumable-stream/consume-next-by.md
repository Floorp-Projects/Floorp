[android-components](../../index.md) / [mozilla.components.support.base.observer](../index.md) / [ConsumableStream](index.md) / [consumeNextBy](./consume-next-by.md)

# consumeNextBy

`@Synchronized fun consumeNextBy(consumers: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<(`[`T`](index.md#T)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/observer/Consumable.kt#L140)

Invokes the given list of lambdas with the next consumable value and marks the
value as consumed if at least one lambda returns true.

### Parameters

`consumers` - the lambdas accepting the next consumable value.

**Return**
true if the consumable was consumed, otherwise false.

