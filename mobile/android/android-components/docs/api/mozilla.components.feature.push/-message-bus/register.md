[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [MessageBus](index.md) / [register](./register.md)

# register

`fun register(type: `[`T`](index.md#T)`, observer: `[`Observer`](../../mozilla.components.concept.push/-bus/-observer/index.md)`<`[`T`](index.md#T)`, `[`M`](index.md#M)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/MessageBus.kt#L20)

Overrides [Bus.register](../../mozilla.components.concept.push/-bus/register.md)

Registers an observer to get notified about events.

### Parameters

`observer` - The observer to be notified for the type.`fun register(type: `[`T`](index.md#T)`, observer: `[`Observer`](../../mozilla.components.concept.push/-bus/-observer/index.md)`<`[`T`](index.md#T)`, `[`M`](index.md#M)`>, owner: LifecycleOwner, autoPause: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/MessageBus.kt#L35)

Overrides [Bus.register](../../mozilla.components.concept.push/-bus/register.md)

Registers an observer to get notified about events.

### Parameters

`observer` - The observer to be notified for the type.

`owner` - the lifecycle owner the provided observer is bound to.

`autoPause` - whether or not the observer should automatically be
paused/resumed with the bound lifecycle.