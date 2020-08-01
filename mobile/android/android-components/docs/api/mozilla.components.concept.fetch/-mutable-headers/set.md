[android-components](../../index.md) / [mozilla.components.concept.fetch](../index.md) / [MutableHeaders](index.md) / [set](./set.md)

# set

`fun set(index: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, header: `[`Header`](../-header/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/fetch/src/main/java/mozilla/components/concept/fetch/Headers.kt#L120)

Overrides [Headers.set](../-headers/set.md)

Sets the [Header](../-header/index.md) at the specified [index](set.md#mozilla.components.concept.fetch.MutableHeaders$set(kotlin.Int, mozilla.components.concept.fetch.Header)/index).

`fun set(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, value: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`MutableHeaders`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/fetch/src/main/java/mozilla/components/concept/fetch/Headers.kt#L152)

Set the only occurrence of the header; potentially overriding an already existing header.

