[android-components](../../index.md) / [mozilla.components.lib.jexl.grammar](../index.md) / [GrammarElement](./index.md)

# GrammarElement

`data class GrammarElement` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/jexl/src/main/java/mozilla/components/lib/jexl/grammar/Grammar.kt#L136)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `GrammarElement(type: `[`Type`](../../mozilla.components.lib.jexl.lexer/-token/-type/index.md)`, precedence: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0, evaluate: (`[`JexlValue`](../../mozilla.components.lib.jexl.value/-jexl-value/index.md)`, `[`JexlValue`](../../mozilla.components.lib.jexl.value/-jexl-value/index.md)`) -> `[`JexlValue`](../../mozilla.components.lib.jexl.value/-jexl-value/index.md)` = null)` |

### Properties

| Name | Summary |
|---|---|
| [evaluate](evaluate.md) | `val evaluate: (`[`JexlValue`](../../mozilla.components.lib.jexl.value/-jexl-value/index.md)`, `[`JexlValue`](../../mozilla.components.lib.jexl.value/-jexl-value/index.md)`) -> `[`JexlValue`](../../mozilla.components.lib.jexl.value/-jexl-value/index.md) |
| [precedence](precedence.md) | `val precedence: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [type](type.md) | `val type: `[`Type`](../../mozilla.components.lib.jexl.lexer/-token/-type/index.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
