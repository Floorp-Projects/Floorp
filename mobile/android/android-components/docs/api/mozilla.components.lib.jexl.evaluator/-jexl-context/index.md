[android-components](../../index.md) / [mozilla.components.lib.jexl.evaluator](../index.md) / [JexlContext](./index.md)

# JexlContext

`class JexlContext` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/jexl/src/main/java/mozilla/components/lib/jexl/evaluator/JexlContext.kt#L38)

Variables defined in the [JexlContext](./index.md) are available to expressions.

Example context:

val context = JexlContext(
    "employees" to JexlArray(
        JexlObject(
            "first" to "Sterling".toJexl(),
            "last" to "Archer".toJexl(),
            "age" to 36.toJexl()),
        JexlObject(
            "first" to "Malory".toJexl(),
            "last" to "Archer".toJexl(),
            "age" to 75.toJexl()),
        JexlObject(
            "first" to "Malory".toJexl(),
            "last" to "Archer".toJexl(),
            "age" to 33.toJexl())
    )
)

This context can be accessed in an JEXL expression like this:

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `JexlContext(vararg pairs: `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`JexlValue`](../../mozilla.components.lib.jexl.value/-jexl-value/index.md)`>)`<br>Variables defined in the [JexlContext](./index.md) are available to expressions. |

### Functions

| Name | Summary |
|---|---|
| [get](get.md) | `fun get(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`JexlValue`](../../mozilla.components.lib.jexl.value/-jexl-value/index.md) |
| [set](set.md) | `fun set(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, value: `[`JexlValue`](../../mozilla.components.lib.jexl.value/-jexl-value/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
