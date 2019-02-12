[android-components](../../index.md) / [mozilla.components.lib.jexl](../index.md) / [Jexl](index.md) / [evaluateBooleanExpression](./evaluate-boolean-expression.md)

# evaluateBooleanExpression

`fun evaluateBooleanExpression(expression: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, context: `[`JexlContext`](../../mozilla.components.lib.jexl.evaluator/-jexl-context/index.md)` = JexlContext(), defaultValue: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`? = null): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/jexl/src/main/java/mozilla/components/lib/jexl/Jexl.kt#L73)

Evaluates a Jexl string with an optional context to a Boolean result. Optionally a default
value can be provided that will be returned in the expression does not return a boolean
result.

