[android-components](../../index.md) / [mozilla.components.lib.jexl.lexer](../index.md) / [Token](./index.md)

# Token

`data class Token` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/jexl/src/main/java/mozilla/components/lib/jexl/lexer/Token.kt#L10)

A token emitted by the [Lexer](#).

### Types

| Name | Summary |
|---|---|
| [Type](-type/index.md) | `enum class Type` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Token(type: `[`Type`](-type/index.md)`, raw: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, value: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`)`<br>A token emitted by the [Lexer](#). |

### Properties

| Name | Summary |
|---|---|
| [raw](raw.md) | `val raw: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [type](type.md) | `val type: `[`Type`](-type/index.md) |
| [value](value.md) | `val value: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
