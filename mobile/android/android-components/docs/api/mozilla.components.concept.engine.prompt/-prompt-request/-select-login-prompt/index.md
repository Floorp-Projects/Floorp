[android-components](../../../index.md) / [mozilla.components.concept.engine.prompt](../../index.md) / [PromptRequest](../index.md) / [SelectLoginPrompt](./index.md)

# SelectLoginPrompt

`data class SelectLoginPrompt : `[`PromptRequest`](../index.md)`, `[`Dismissible`](../-dismissible/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/prompt/PromptRequest.kt#L90)

Value type that represents a request for a select login prompt.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SelectLoginPrompt(logins: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../../../mozilla.components.concept.storage/-login/index.md)`>, onDismiss: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onConfirm: (`[`Login`](../../../mozilla.components.concept.storage/-login/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>Value type that represents a request for a select login prompt. |

### Properties

| Name | Summary |
|---|---|
| [logins](logins.md) | `val logins: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../../../mozilla.components.concept.storage/-login/index.md)`>`<br>a list of logins that are associated with the current domain. |
| [onConfirm](on-confirm.md) | `val onConfirm: (`[`Login`](../../../mozilla.components.concept.storage/-login/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>callback that is called when the user wants to save the login. |
| [onDismiss](on-dismiss.md) | `val onDismiss: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>callback to let the page know the user dismissed the dialog. |
