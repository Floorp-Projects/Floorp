[android-components](../../index.md) / [mozilla.components.support.base.observer](../index.md) / [ConsumableStream](index.md) / [consumeNext](./consume-next.md)

# consumeNext

`@Synchronized fun consumeNext(consumer: (value: `[`T`](index.md#T)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/observer/Consumable.kt#L114)

Invokes the given lambda with the next consumable value and marks the value
as consumed if the lambda returns true.

### Parameters

`consumer` - a lambda accepting a consumable value.

**Return**
true if the consumable was consumed, otherwise false.

