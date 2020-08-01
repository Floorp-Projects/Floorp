[android-components](../../index.md) / [mozilla.components.browser.engine.system.matcher](../index.md) / [ReversibleString](./index.md)

# ReversibleString

`abstract class ReversibleString` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/matcher/ReversibleString.kt#L14)

A String wrapper utility that allows for efficient string reversal. We
regularly need to reverse strings. The standard way of doing this in Java
would be to copy the string to reverse (e.g. using StringBuffer.reverse()).
This seems wasteful when we only read our Strings character by character,
in which case can just transpose positions as needed.

### Properties

| Name | Summary |
|---|---|
| [isReversed](is-reversed.md) | `abstract val isReversed: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [offsetEnd](offset-end.md) | `val offsetEnd: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [offsetStart](offset-start.md) | `val offsetStart: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [string](string.md) | `val string: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Functions

| Name | Summary |
|---|---|
| [charAt](char-at.md) | `abstract fun charAt(position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Char`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char/index.html) |
| [length](length.md) | `fun length(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Returns the length of this string. |
| [reverse](reverse.md) | `fun reverse(): `[`ReversibleString`](./index.md)<br>Reverses this string. |
| [substring](substring.md) | `abstract fun substring(startIndex: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`ReversibleString`](./index.md) |

### Companion Object Functions

| Name | Summary |
|---|---|
| [create](create.md) | `fun create(string: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`ReversibleString`](./index.md)<br>Create a [ReversibleString](./index.md) for the provided [String](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
