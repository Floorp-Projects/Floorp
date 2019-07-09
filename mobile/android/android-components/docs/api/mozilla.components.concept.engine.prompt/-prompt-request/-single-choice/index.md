[android-components](../../../index.md) / [mozilla.components.concept.engine.prompt](../../index.md) / [PromptRequest](../index.md) / [SingleChoice](./index.md)

# SingleChoice

`data class SingleChoice : `[`PromptRequest`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/prompt/PromptRequest.kt#L23)

Value type that represents a request for a single choice prompt.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SingleChoice(choices: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`Choice`](../../-choice/index.md)`>, onConfirm: (`[`Choice`](../../-choice/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>Value type that represents a request for a single choice prompt. |

### Properties

| Name | Summary |
|---|---|
| [choices](choices.md) | `val choices: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`Choice`](../../-choice/index.md)`>`<br>All the possible options. |
| [onConfirm](on-confirm.md) | `val onConfirm: (`[`Choice`](../../-choice/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A callback indicating which option was selected. |
