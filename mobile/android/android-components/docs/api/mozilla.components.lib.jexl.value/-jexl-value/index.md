[android-components](../../index.md) / [mozilla.components.lib.jexl.value](../index.md) / [JexlValue](./index.md)

# JexlValue

`sealed class JexlValue` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/jexl/src/main/java/mozilla/components/lib/jexl/value/JexlValue.kt#L12)

A JEXL value type.

### Properties

| Name | Summary |
|---|---|
| [value](value.md) | `abstract val value: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html) |

### Functions

| Name | Summary |
|---|---|
| [compareTo](compare-to.md) | `abstract operator fun compareTo(other: `[`JexlValue`](./index.md)`): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [div](div.md) | `abstract operator fun div(other: `[`JexlValue`](./index.md)`): `[`JexlValue`](./index.md) |
| [plus](plus.md) | `abstract operator fun plus(other: `[`JexlValue`](./index.md)`): `[`JexlValue`](./index.md) |
| [times](times.md) | `abstract operator fun times(other: `[`JexlValue`](./index.md)`): `[`JexlValue`](./index.md) |
| [toBoolean](to-boolean.md) | `abstract fun toBoolean(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [JexlArray](../-jexl-array/index.md) | `class JexlArray : `[`JexlValue`](./index.md)<br>JEXL Array type. |
| [JexlBoolean](../-jexl-boolean/index.md) | `class JexlBoolean : `[`JexlValue`](./index.md)<br>JEXL Boolean type. |
| [JexlDouble](../-jexl-double/index.md) | `class JexlDouble : `[`JexlValue`](./index.md)<br>JEXL Double type. |
| [JexlInteger](../-jexl-integer/index.md) | `class JexlInteger : `[`JexlValue`](./index.md)<br>JEXL Integer type. |
| [JexlObject](../-jexl-object/index.md) | `class JexlObject : `[`JexlValue`](./index.md)<br>JEXL Object type. |
| [JexlString](../-jexl-string/index.md) | `class JexlString : `[`JexlValue`](./index.md)<br>JEXL String type. |
| [JexlUndefined](../-jexl-undefined/index.md) | `class JexlUndefined : `[`JexlValue`](./index.md)<br>JEXL undefined type. |
