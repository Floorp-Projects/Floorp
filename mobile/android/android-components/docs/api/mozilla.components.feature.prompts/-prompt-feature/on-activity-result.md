[android-components](../../index.md) / [mozilla.components.feature.prompts](../index.md) / [PromptFeature](index.md) / [onActivityResult](./on-activity-result.md)

# onActivityResult

`fun onActivityResult(requestCode: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, resultCode: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, intent: <ERROR CLASS>?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/prompts/src/main/java/mozilla/components/feature/prompts/PromptFeature.kt#L153)

Notifies the feature of intent results for prompt requests handled by
other apps like file chooser requests.

### Parameters

`requestCode` - The code of the app that requested the intent.

`intent` - The result of the request.