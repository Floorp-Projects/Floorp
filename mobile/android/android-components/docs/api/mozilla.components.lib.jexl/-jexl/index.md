[android-components](../../index.md) / [mozilla.components.lib.jexl](../index.md) / [Jexl](./index.md)

# Jexl

`class Jexl` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/jexl/src/main/java/mozilla/components/lib/jexl/Jexl.kt#L19)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Jexl(grammar: `[`Grammar`](../../mozilla.components.lib.jexl.grammar/-grammar/index.md)` = Grammar())` |

### Functions

| Name | Summary |
|---|---|
| [addTransform](add-transform.md) | `fun addTransform(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, transform: `[`Transform`](../../mozilla.components.lib.jexl.evaluator/-transform.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds or replaces a transform function in this Jexl instance. |
| [evaluate](evaluate.md) | `fun evaluate(expression: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, context: `[`JexlContext`](../../mozilla.components.lib.jexl.evaluator/-jexl-context/index.md)` = JexlContext()): `[`JexlValue`](../../mozilla.components.lib.jexl.value/-jexl-value/index.md)<br>Evaluates a Jexl string within an optional context. |
| [evaluateBooleanExpression](evaluate-boolean-expression.md) | `fun evaluateBooleanExpression(expression: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, context: `[`JexlContext`](../../mozilla.components.lib.jexl.evaluator/-jexl-context/index.md)` = JexlContext(), defaultValue: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`? = null): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Evaluates a Jexl string with an optional context to a Boolean result. Optionally a default value can be provided that will be returned in the expression does not return a boolean result. |
