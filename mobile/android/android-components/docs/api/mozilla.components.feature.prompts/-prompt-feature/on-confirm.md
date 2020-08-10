[android-components](../../index.md) / [mozilla.components.feature.prompts](../index.md) / [PromptFeature](index.md) / [onConfirm](./on-confirm.md)

# onConfirm

`fun onConfirm(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, value: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/prompts/src/main/java/mozilla/components/feature/prompts/PromptFeature.kt#L326)

Invoked when the user confirms the action on the dialog. This consumes
the [PromptFeature](index.md) value from the [SessionState](../../mozilla.components.browser.state.state/-session-state/index.md) indicated by [sessionId](on-confirm.md#mozilla.components.feature.prompts.PromptFeature$onConfirm(kotlin.String, kotlin.Any)/sessionId).

### Parameters

`sessionId` - that requested to show the dialog.

`value` - an optional value provided by the dialog as a result of confirming the action.