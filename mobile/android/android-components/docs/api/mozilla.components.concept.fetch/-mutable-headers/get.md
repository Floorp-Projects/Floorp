[android-components](../../index.md) / [mozilla.components.concept.fetch](../index.md) / [MutableHeaders](index.md) / [get](./get.md)

# get

`fun get(index: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Header`](../-header/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/fetch/src/main/java/mozilla/components/concept/fetch/Headers.kt#L102)

Overrides [Headers.get](../-headers/get.md)

Gets the [Header](../-header/index.md) at the specified [index](get.md#mozilla.components.concept.fetch.MutableHeaders$get(kotlin.Int)/index).

`fun get(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/fetch/src/main/java/mozilla/components/concept/fetch/Headers.kt#L107)

Overrides [Headers.get](../-headers/get.md)

Returns the last value corresponding to the specified header field name. Or null if the header does not exist.

