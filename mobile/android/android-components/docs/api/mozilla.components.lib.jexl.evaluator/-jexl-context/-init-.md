[android-components](../../index.md) / [mozilla.components.lib.jexl.evaluator](../index.md) / [JexlContext](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`JexlContext(vararg pairs: `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`JexlValue`](../../mozilla.components.lib.jexl.value/-jexl-value/index.md)`>)`

Variables defined in the [JexlContext](index.md) are available to expressions.

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

