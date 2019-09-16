[android-components](../../index.md) / [mozilla.components.support.base.observer](../index.md) / [Consumable](index.md) / [consume](./consume.md)

# consume

`@Synchronized fun consume(consumer: (value: `[`T`](index.md#T)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/observer/Consumable.kt#L30)

Invokes the given lambda and marks the value as consumed if the lambda returns true.

