[android-components](../../index.md) / [mozilla.components.feature.prompts](../index.md) / [PromptFeature](index.md) / [onClear](./on-clear.md)

# onClear

`fun onClear(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/prompts/src/main/java/mozilla/components/feature/prompts/PromptFeature.kt#L434)

Invoked when the user is requesting to clear the selected value from the dialog.
This consumes the [PromptFeature](index.md) value from the [SessionState](../../mozilla.components.browser.state.state/-session-state/index.md) indicated by [sessionId](on-clear.md#mozilla.components.feature.prompts.PromptFeature$onClear(kotlin.String)/sessionId).

### Parameters

`sessionId` - that requested to show the dialog.