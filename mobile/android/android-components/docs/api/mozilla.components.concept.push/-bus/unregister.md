[android-components](../../index.md) / [mozilla.components.concept.push](../index.md) / [Bus](index.md) / [unregister](./unregister.md)

# unregister

`abstract fun unregister(type: `[`T`](index.md#T)`, observer: `[`Observer`](-observer/index.md)`<`[`T`](index.md#T)`, `[`M`](index.md#M)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/push/src/main/java/mozilla/components/concept/push/Bus.kt#L55)

Unregisters an observer to stop getting notified about events.

### Parameters

`observer` - The observer that no longer wants to be notified.