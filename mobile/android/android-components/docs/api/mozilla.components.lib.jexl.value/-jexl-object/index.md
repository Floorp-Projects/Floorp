[android-components](../../index.md) / [mozilla.components.lib.jexl.value](../index.md) / [JexlObject](./index.md)

# JexlObject

`class JexlObject : `[`JexlValue`](../-jexl-value/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/jexl/src/main/java/mozilla/components/lib/jexl/value/JexlValue.kt#L246)

JEXL Object type.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `JexlObject(vararg pairs: `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`JexlValue`](../-jexl-value/index.md)`>)``JexlObject(value: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`JexlValue`](../-jexl-value/index.md)`>)`<br>JEXL Object type. |

### Properties

| Name | Summary |
|---|---|
| [value](value.md) | `val value: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`JexlValue`](../-jexl-value/index.md)`>` |

### Functions

| Name | Summary |
|---|---|
| [compareTo](compare-to.md) | `fun compareTo(other: `[`JexlValue`](../-jexl-value/index.md)`): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [div](div.md) | `fun div(other: `[`JexlValue`](../-jexl-value/index.md)`): `[`JexlValue`](../-jexl-value/index.md) |
| [equals](equals.md) | `fun equals(other: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [hashCode](hash-code.md) | `fun hashCode(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [plus](plus.md) | `fun plus(other: `[`JexlValue`](../-jexl-value/index.md)`): `[`JexlValue`](../-jexl-value/index.md) |
| [times](times.md) | `fun times(other: `[`JexlValue`](../-jexl-value/index.md)`): `[`JexlValue`](../-jexl-value/index.md) |
| [toBoolean](to-boolean.md) | `fun toBoolean(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [toString](to-string.md) | `fun toString(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
