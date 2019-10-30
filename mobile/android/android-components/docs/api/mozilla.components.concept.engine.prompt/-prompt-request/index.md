[android-components](../../index.md) / [mozilla.components.concept.engine.prompt](../index.md) / [PromptRequest](./index.md)

# PromptRequest

`sealed class PromptRequest` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/prompt/PromptRequest.kt#L17)

Value type that represents a request for showing a native dialog for prompt web content.

### Types

| Name | Summary |
|---|---|
| [Alert](-alert/index.md) | `data class Alert : `[`PromptRequest`](./index.md)`, `[`Dismissible`](-dismissible/index.md)<br>Value type that represents a request for an alert prompt. |
| [Authentication](-authentication/index.md) | `data class Authentication : `[`PromptRequest`](./index.md)`, `[`Dismissible`](-dismissible/index.md)<br>Value type that represents a request for an authentication prompt. For more related info take a look at &lt;a href="https://developer.mozilla.org/en-US/docs/Web/HTTP/Authentication&gt;MDN docs |
| [Color](-color/index.md) | `data class Color : `[`PromptRequest`](./index.md)`, `[`Dismissible`](-dismissible/index.md)<br>Value type that represents a request for a selecting one or multiple files. |
| [Confirm](-confirm/index.md) | `data class Confirm : `[`PromptRequest`](./index.md)`, `[`Dismissible`](-dismissible/index.md)<br>Value type that represents a request for showing a &lt;a href="https://developer.mozilla.org/en-US/docs/Web/API/Window/confirm&gt;confirm prompt. |
| [Dismissible](-dismissible/index.md) | `interface Dismissible` |
| [File](-file/index.md) | `data class File : `[`PromptRequest`](./index.md)<br>Value type that represents a request for a selecting one or multiple files. |
| [MenuChoice](-menu-choice/index.md) | `data class MenuChoice : `[`PromptRequest`](./index.md)<br>Value type that represents a request for a menu choice prompt. |
| [MultipleChoice](-multiple-choice/index.md) | `data class MultipleChoice : `[`PromptRequest`](./index.md)<br>Value type that represents a request for a multiple choice prompt. |
| [Popup](-popup/index.md) | `data class Popup : `[`PromptRequest`](./index.md)<br>Value type that represents a request for showing a pop-pup prompt. This occurs when content attempts to open a new window, in a way that doesn't appear to be the result of user input. |
| [SingleChoice](-single-choice/index.md) | `data class SingleChoice : `[`PromptRequest`](./index.md)<br>Value type that represents a request for a single choice prompt. |
| [TextPrompt](-text-prompt/index.md) | `data class TextPrompt : `[`PromptRequest`](./index.md)`, `[`Dismissible`](-dismissible/index.md)<br>Value type that represents a request for an alert prompt to enter a message. |
| [TimeSelection](-time-selection/index.md) | `class TimeSelection : `[`PromptRequest`](./index.md)<br>Value type that represents a request for a date prompt for picking a year, month, and day. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [Alert](-alert/index.md) | `data class Alert : `[`PromptRequest`](./index.md)`, `[`Dismissible`](-dismissible/index.md)<br>Value type that represents a request for an alert prompt. |
| [Authentication](-authentication/index.md) | `data class Authentication : `[`PromptRequest`](./index.md)`, `[`Dismissible`](-dismissible/index.md)<br>Value type that represents a request for an authentication prompt. For more related info take a look at &lt;a href="https://developer.mozilla.org/en-US/docs/Web/HTTP/Authentication&gt;MDN docs |
| [Color](-color/index.md) | `data class Color : `[`PromptRequest`](./index.md)`, `[`Dismissible`](-dismissible/index.md)<br>Value type that represents a request for a selecting one or multiple files. |
| [Confirm](-confirm/index.md) | `data class Confirm : `[`PromptRequest`](./index.md)`, `[`Dismissible`](-dismissible/index.md)<br>Value type that represents a request for showing a &lt;a href="https://developer.mozilla.org/en-US/docs/Web/API/Window/confirm&gt;confirm prompt. |
| [File](-file/index.md) | `data class File : `[`PromptRequest`](./index.md)<br>Value type that represents a request for a selecting one or multiple files. |
| [MenuChoice](-menu-choice/index.md) | `data class MenuChoice : `[`PromptRequest`](./index.md)<br>Value type that represents a request for a menu choice prompt. |
| [MultipleChoice](-multiple-choice/index.md) | `data class MultipleChoice : `[`PromptRequest`](./index.md)<br>Value type that represents a request for a multiple choice prompt. |
| [Popup](-popup/index.md) | `data class Popup : `[`PromptRequest`](./index.md)<br>Value type that represents a request for showing a pop-pup prompt. This occurs when content attempts to open a new window, in a way that doesn't appear to be the result of user input. |
| [SingleChoice](-single-choice/index.md) | `data class SingleChoice : `[`PromptRequest`](./index.md)<br>Value type that represents a request for a single choice prompt. |
| [TextPrompt](-text-prompt/index.md) | `data class TextPrompt : `[`PromptRequest`](./index.md)`, `[`Dismissible`](-dismissible/index.md)<br>Value type that represents a request for an alert prompt to enter a message. |
| [TimeSelection](-time-selection/index.md) | `class TimeSelection : `[`PromptRequest`](./index.md)<br>Value type that represents a request for a date prompt for picking a year, month, and day. |
