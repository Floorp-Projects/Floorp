[android-components](../../index.md) / [mozilla.components.concept.fetch](../index.md) / [Headers](./index.md)

# Headers

`interface Headers : `[`Iterable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-iterable/index.html)`<`[`Header`](../-header/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/fetch/src/main/java/mozilla/components/concept/fetch/Headers.kt#L10)

A collection of HTTP [Headers](./index.md) (immutable) of a [Request](../-request/index.md) or [Response](../-response/index.md).

### Types

| Name | Summary |
|---|---|
| [Names](-names/index.md) | `object Names`<br>A collection of common HTTP header names. |
| [Values](-values/index.md) | `object Values`<br>A collection of common HTTP header values. |

### Properties

| Name | Summary |
|---|---|
| [size](size.md) | `abstract val size: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Returns the number of headers (key / value combinations). |

### Functions

| Name | Summary |
|---|---|
| [contains](contains.md) | `abstract operator fun contains(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if a [Header](../-header/index.md) with the given [name](contains.md#mozilla.components.concept.fetch.Headers$contains(kotlin.String)/name) exists. |
| [get](get.md) | `abstract operator fun get(index: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Header`](../-header/index.md)<br>Gets the [Header](../-header/index.md) at the specified [index](get.md#mozilla.components.concept.fetch.Headers$get(kotlin.Int)/index).`abstract operator fun get(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Returns the last values corresponding to the specified header field name. Or null if the header does not exist. |
| [getAll](get-all.md) | `abstract fun getAll(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>Returns the list of values corresponding to the specified header field name. |
| [set](set.md) | `abstract operator fun set(index: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, header: `[`Header`](../-header/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the [Header](../-header/index.md) at the specified [index](set.md#mozilla.components.concept.fetch.Headers$set(kotlin.Int, mozilla.components.concept.fetch.Header)/index). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
| [toJSONArray](../../mozilla.components.support.ktx.android.org.json/kotlin.collections.-iterable/to-j-s-o-n-array.md) | `fun `[`Iterable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-iterable/index.html)`<`[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`>.toJSONArray(): <ERROR CLASS>` |

### Inheritors

| Name | Summary |
|---|---|
| [MutableHeaders](../-mutable-headers/index.md) | `class MutableHeaders : `[`Headers`](./index.md)`, `[`MutableIterable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-mutable-iterable/index.html)`<`[`Header`](../-header/index.md)`>`<br>A collection of HTTP [Headers](./index.md) (mutable) of a [Request](../-request/index.md) or [Response](../-response/index.md). |
