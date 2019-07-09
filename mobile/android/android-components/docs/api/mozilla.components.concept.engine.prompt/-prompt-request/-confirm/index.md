[android-components](../../../index.md) / [mozilla.components.concept.engine.prompt](../../index.md) / [PromptRequest](../index.md) / [Confirm](./index.md)

# Confirm

`data class Confirm : `[`PromptRequest`](../index.md)`, `[`Dismissible`](../-dismissible/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/prompt/PromptRequest.kt#L223)

Value type that represents a request for showing a
&lt;a href="https://developer.mozilla.org/en-US/docs/Web/API/Window/confirm&gt;confirm prompt.

The prompt can have up to three buttons, they could be positive, negative and neutral.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Confirm(title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, message: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, hasShownManyDialogs: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, positiveButtonTitle: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", negativeButtonTitle: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", neutralButtonTitle: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", onConfirmPositiveButton: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onConfirmNegativeButton: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onConfirmNeutralButton: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onDismiss: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>Value type that represents a request for showing a &lt;a href="https://developer.mozilla.org/en-US/docs/Web/API/Window/confirm&gt;confirm prompt. |

### Properties

| Name | Summary |
|---|---|
| [hasShownManyDialogs](has-shown-many-dialogs.md) | `val hasShownManyDialogs: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>tells if this page has shown multiple prompts within a short period of time. |
| [message](message.md) | `val message: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the body of the dialog. |
| [negativeButtonTitle](negative-button-title.md) | `val negativeButtonTitle: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>optional title for the negative button. |
| [neutralButtonTitle](neutral-button-title.md) | `val neutralButtonTitle: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>optional title for the neutral button. |
| [onConfirmNegativeButton](on-confirm-negative-button.md) | `val onConfirmNegativeButton: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>callback to notify that the user has clicked the negative button. |
| [onConfirmNeutralButton](on-confirm-neutral-button.md) | `val onConfirmNeutralButton: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>callback to notify that the user has clicked the neutral button. |
| [onConfirmPositiveButton](on-confirm-positive-button.md) | `val onConfirmPositiveButton: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>callback to notify that the user has clicked the positive button. |
| [onDismiss](on-dismiss.md) | `val onDismiss: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>callback to notify that the user has canceled the dialog. |
| [positiveButtonTitle](positive-button-title.md) | `val positiveButtonTitle: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>optional title for the positive button. |
| [title](title.md) | `val title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>of the dialog. |
