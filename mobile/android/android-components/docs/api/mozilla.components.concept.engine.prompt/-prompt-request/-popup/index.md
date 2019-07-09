[android-components](../../../index.md) / [mozilla.components.concept.engine.prompt](../../index.md) / [PromptRequest](../index.md) / [Popup](./index.md)

# Popup

`data class Popup : `[`PromptRequest`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/prompt/PromptRequest.kt#L200)

Value type that represents a request for showing a pop-pup prompt.
This occurs when content attempts to open a new window,
in a way that doesn't appear to be the result of user input.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Popup(targetUri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, onAllow: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onDeny: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>Value type that represents a request for showing a pop-pup prompt. This occurs when content attempts to open a new window, in a way that doesn't appear to be the result of user input. |

### Properties

| Name | Summary |
|---|---|
| [onAllow](on-allow.md) | `val onAllow: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>callback to notify that the user wants to open the [targetUri](target-uri.md). |
| [onDeny](on-deny.md) | `val onDeny: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>callback to notify that the user doesn't want to open the [targetUri](target-uri.md). |
| [targetUri](target-uri.md) | `val targetUri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the uri that the page is trying to open. |
