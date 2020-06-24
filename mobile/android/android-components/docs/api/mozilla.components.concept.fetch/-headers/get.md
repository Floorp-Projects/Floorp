[android-components](../../index.md) / [mozilla.components.concept.fetch](../index.md) / [Headers](index.md) / [get](./get.md)

# get

`abstract operator fun get(index: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Header`](../-header/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/fetch/src/main/java/mozilla/components/concept/fetch/Headers.kt#L19)

Gets the [Header](../-header/index.md) at the specified [index](get.md#mozilla.components.concept.fetch.Headers$get(kotlin.Int)/index).

`abstract operator fun get(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/fetch/src/main/java/mozilla/components/concept/fetch/Headers.kt#L24)

Returns the last values corresponding to the specified header field name. Or null if the header does not exist.

