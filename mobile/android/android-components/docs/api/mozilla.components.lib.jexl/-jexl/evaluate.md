[android-components](../../index.md) / [mozilla.components.lib.jexl](../index.md) / [Jexl](index.md) / [evaluate](./evaluate.md)

# evaluate

`fun evaluate(expression: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, context: `[`JexlContext`](../../mozilla.components.lib.jexl.evaluator/-jexl-context/index.md)` = JexlContext()): `[`JexlValue`](../../mozilla.components.lib.jexl.value/-jexl-value/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/jexl/src/main/java/mozilla/components/lib/jexl/Jexl.kt#L49)

Evaluates a Jexl string within an optional context.

### Parameters

`expression` - The Jexl expression to be evaluated.

`context` - A mapping of variables to values, which will be made accessible to the Jexl
    expression when evaluating it.

### Exceptions

`JexlException` - if lexing, parsing or evaluating the expression failed.

**Return**
The result of the evaluation.

