[android-components](../../index.md) / [mozilla.components.feature.prompts.share](../index.md) / [ShareDelegate](index.md) / [showShareSheet](./show-share-sheet.md)

# showShareSheet

`abstract fun showShareSheet(context: <ERROR CLASS>, shareData: `[`ShareData`](../../mozilla.components.concept.engine.prompt/-share-data/index.md)`, onDismiss: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onSuccess: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/prompts/src/main/java/mozilla/components/feature/prompts/share/ShareDelegate.kt#L25)

Displays a share sheet for the given [ShareData](../../mozilla.components.concept.engine.prompt/-share-data/index.md).

### Parameters

`context` - Reference to context.

`shareData` - Data to share.

`onDismiss` - Callback to be invoked if the share sheet is dismissed and nothing
is selected, or if it fails to load.

`onSuccess` - Callback to be invoked if the data is successfully shared.