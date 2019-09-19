[android-components](../../index.md) / [mozilla.components.support.base.observer](../index.md) / [Consumable](index.md) / [consumeBy](./consume-by.md)

# consumeBy

`@Synchronized fun consumeBy(consumers: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<(`[`T`](index.md#T)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/observer/Consumable.kt#L45)

Invokes the given list of lambdas and marks the value as consumed if at least one lambda
returns true.

