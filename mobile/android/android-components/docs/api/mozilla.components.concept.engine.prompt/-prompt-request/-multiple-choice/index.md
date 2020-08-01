[android-components](../../../index.md) / [mozilla.components.concept.engine.prompt](../../index.md) / [PromptRequest](../index.md) / [MultipleChoice](./index.md)

# MultipleChoice

`data class MultipleChoice : `[`PromptRequest`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/prompt/PromptRequest.kt#L31)

Value type that represents a request for a multiple choice prompt.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `MultipleChoice(choices: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`Choice`](../../-choice/index.md)`>, onConfirm: (`[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`Choice`](../../-choice/index.md)`>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>Value type that represents a request for a multiple choice prompt. |

### Properties

| Name | Summary |
|---|---|
| [choices](choices.md) | `val choices: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`Choice`](../../-choice/index.md)`>`<br>All the possible options. |
| [onConfirm](on-confirm.md) | `val onConfirm: (`[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`Choice`](../../-choice/index.md)`>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A callback indicating witch options has been selected. |
