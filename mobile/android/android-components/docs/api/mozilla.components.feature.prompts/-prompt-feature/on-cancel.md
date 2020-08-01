[android-components](../../index.md) / [mozilla.components.feature.prompts](../index.md) / [PromptFeature](index.md) / [onCancel](./on-cancel.md)

# onCancel

`fun onCancel(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/prompts/src/main/java/mozilla/components/feature/prompts/PromptFeature.kt#L308)

Invoked when a dialog is dismissed. This consumes the [PromptFeature](index.md)
value from the session indicated by [sessionId](on-cancel.md#mozilla.components.feature.prompts.PromptFeature$onCancel(kotlin.String)/sessionId).

### Parameters

`sessionId` - this is the id of the session which requested the prompt.