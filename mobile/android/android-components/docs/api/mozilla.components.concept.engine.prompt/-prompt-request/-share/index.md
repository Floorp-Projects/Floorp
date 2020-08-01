[android-components](../../../index.md) / [mozilla.components.concept.engine.prompt](../../index.md) / [PromptRequest](../index.md) / [Share](./index.md)

# Share

`data class Share : `[`PromptRequest`](../index.md)`, `[`Dismissible`](../-dismissible/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/prompt/PromptRequest.kt#L285)

Value type that represents a request to share data.
https://w3c.github.io/web-share/

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Share(data: `[`ShareData`](../../-share-data/index.md)`, onSuccess: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onFailure: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onDismiss: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>Value type that represents a request to share data. https://w3c.github.io/web-share/ |

### Properties

| Name | Summary |
|---|---|
| [data](data.md) | `val data: `[`ShareData`](../../-share-data/index.md)<br>Share data containing title, text, and url of the request. |
| [onDismiss](on-dismiss.md) | `val onDismiss: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Callback to notify that the user aborted the share. |
| [onFailure](on-failure.md) | `val onFailure: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Callback to notify that the user attempted to share with another app, but it failed. |
| [onSuccess](on-success.md) | `val onSuccess: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Callback to notify that the user hared with another app successfully. |
