[android-components](../index.md) / [mozilla.components.lib.state.ext](index.md) / [observeForever](./observe-forever.md)

# observeForever

`fun <S : `[`State`](../mozilla.components.lib.state/-state.md)`, A : `[`Action`](../mozilla.components.lib.state/-action.md)`> `[`Store`](../mozilla.components.lib.state/-store/index.md)`<`[`S`](observe-forever.md#S)`, `[`A`](observe-forever.md#A)`>.observeForever(observer: `[`Observer`](../mozilla.components.lib.state/-observer.md)`<`[`S`](observe-forever.md#S)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/state/src/main/java/mozilla/components/lib/state/ext/StoreExtensions.kt#L92)

Registers an [Observer](../mozilla.components.lib.state/-observer.md) function that will observe the store indefinitely.

Right after registering the [Observer](../mozilla.components.lib.state/-observer.md) will be invoked with the current [State](../mozilla.components.lib.state/-state.md).

