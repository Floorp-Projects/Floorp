[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [MessageBus](index.md) / [unregister](./unregister.md)

# unregister

`fun unregister(type: `[`T`](index.md#T)`, observer: `[`Observer`](../../mozilla.components.concept.push/-bus/-observer/index.md)`<`[`T`](index.md#T)`, `[`M`](index.md#M)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/MessageBus.kt#L50)

Overrides [Bus.unregister](../../mozilla.components.concept.push/-bus/unregister.md)

Unregisters an observer to stop getting notified about events.

### Parameters

`observer` - The observer that no longer wants to be notified.