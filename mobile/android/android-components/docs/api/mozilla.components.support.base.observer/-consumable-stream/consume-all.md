[android-components](../../index.md) / [mozilla.components.support.base.observer](../index.md) / [ConsumableStream](index.md) / [consumeAll](./consume-all.md)

# consumeAll

`@Synchronized fun consumeAll(consumer: (value: `[`T`](index.md#T)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/observer/Consumable.kt#L127)

Invokes the given lambda for each consumable value and marks the values
as consumed if the lambda returns true.

### Parameters

`consumer` - a lambda accepting a consumable value.

**Return**
true if all consumables were consumed, otherwise false.

