[android-components](../../../index.md) / [mozilla.components.concept.engine.prompt](../../index.md) / [PromptRequest](../index.md) / [BeforeUnload](./index.md)

# BeforeUnload

`data class BeforeUnload : `[`PromptRequest`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/prompt/PromptRequest.kt#L64)

BeforeUnloadPrompt represents the onbeforeunload prompt.
This prompt is shown when a user is leaving a website and there is formation pending to be saved.
For more information see https://developer.mozilla.org/en-US/docs/Web/API/WindowEventHandlers/onbeforeunload.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BeforeUnload(title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, onLeave: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onStay: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>BeforeUnloadPrompt represents the onbeforeunload prompt. This prompt is shown when a user is leaving a website and there is formation pending to be saved. For more information see https://developer.mozilla.org/en-US/docs/Web/API/WindowEventHandlers/onbeforeunload. |

### Properties

| Name | Summary |
|---|---|
| [onLeave](on-leave.md) | `val onLeave: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>callback to notify that the user wants leave the site. |
| [onStay](on-stay.md) | `val onStay: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>callback to notify that the user wants stay in the site. |
| [title](title.md) | `val title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>of the dialog. |
