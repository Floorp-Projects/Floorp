[android-components](../../index.md) / [mozilla.components.feature.prompts.share](../index.md) / [ShareDelegate](./index.md)

# ShareDelegate

`interface ShareDelegate` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/prompts/src/main/java/mozilla/components/feature/prompts/share/ShareDelegate.kt#L14)

Delegate to display a share prompt.

### Functions

| Name | Summary |
|---|---|
| [showShareSheet](show-share-sheet.md) | `abstract fun showShareSheet(context: <ERROR CLASS>, shareData: `[`ShareData`](../../mozilla.components.concept.engine.prompt/-share-data/index.md)`, onDismiss: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onSuccess: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Displays a share sheet for the given [ShareData](../../mozilla.components.concept.engine.prompt/-share-data/index.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [DefaultShareDelegate](../-default-share-delegate/index.md) | `class DefaultShareDelegate : `[`ShareDelegate`](./index.md)<br>Default [ShareDelegate](./index.md) implementation that displays the native share sheet. |
