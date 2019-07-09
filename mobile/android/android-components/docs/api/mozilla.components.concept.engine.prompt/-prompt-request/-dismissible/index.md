[android-components](../../../index.md) / [mozilla.components.concept.engine.prompt](../../index.md) / [PromptRequest](../index.md) / [Dismissible](./index.md)

# Dismissible

`interface Dismissible` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/prompt/PromptRequest.kt#L236)

### Properties

| Name | Summary |
|---|---|
| [onDismiss](on-dismiss.md) | `abstract val onDismiss: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [Alert](../-alert/index.md) | `data class Alert : `[`PromptRequest`](../index.md)`, `[`Dismissible`](./index.md)<br>Value type that represents a request for an alert prompt. |
| [Authentication](../-authentication/index.md) | `data class Authentication : `[`PromptRequest`](../index.md)`, `[`Dismissible`](./index.md)<br>Value type that represents a request for an authentication prompt. For more related info take a look at &lt;a href="https://developer.mozilla.org/en-US/docs/Web/HTTP/Authentication&gt;MDN docs |
| [Color](../-color/index.md) | `data class Color : `[`PromptRequest`](../index.md)`, `[`Dismissible`](./index.md)<br>Value type that represents a request for a selecting one or multiple files. |
| [Confirm](../-confirm/index.md) | `data class Confirm : `[`PromptRequest`](../index.md)`, `[`Dismissible`](./index.md)<br>Value type that represents a request for showing a &lt;a href="https://developer.mozilla.org/en-US/docs/Web/API/Window/confirm&gt;confirm prompt. |
| [TextPrompt](../-text-prompt/index.md) | `data class TextPrompt : `[`PromptRequest`](../index.md)`, `[`Dismissible`](./index.md)<br>Value type that represents a request for an alert prompt to enter a message. |
