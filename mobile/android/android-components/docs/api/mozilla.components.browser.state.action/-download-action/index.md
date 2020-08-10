[android-components](../../index.md) / [mozilla.components.browser.state.action](../index.md) / [DownloadAction](./index.md)

# DownloadAction

`sealed class DownloadAction : `[`BrowserAction`](../-browser-action.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L573)

[BrowserAction](../-browser-action.md) implementations related to updating the global download state.

### Types

| Name | Summary |
|---|---|
| [AddDownloadAction](-add-download-action/index.md) | `data class AddDownloadAction : `[`DownloadAction`](./index.md)<br>Updates the [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md) to track the provided [download](-add-download-action/download.md) as added. |
| [RemoveAllDownloadsAction](-remove-all-downloads-action.md) | `object RemoveAllDownloadsAction : `[`DownloadAction`](./index.md)<br>Updates the [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md) to remove all downloads. |
| [RemoveDownloadAction](-remove-download-action/index.md) | `data class RemoveDownloadAction : `[`DownloadAction`](./index.md)<br>Updates the [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md) to remove the download with the provided [downloadId](-remove-download-action/download-id.md). |
| [UpdateDownloadAction](-update-download-action/index.md) | `data class UpdateDownloadAction : `[`DownloadAction`](./index.md)<br>Updates the provided [download](-update-download-action/download.md) on the [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [AddDownloadAction](-add-download-action/index.md) | `data class AddDownloadAction : `[`DownloadAction`](./index.md)<br>Updates the [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md) to track the provided [download](-add-download-action/download.md) as added. |
| [RemoveAllDownloadsAction](-remove-all-downloads-action.md) | `object RemoveAllDownloadsAction : `[`DownloadAction`](./index.md)<br>Updates the [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md) to remove all downloads. |
| [RemoveDownloadAction](-remove-download-action/index.md) | `data class RemoveDownloadAction : `[`DownloadAction`](./index.md)<br>Updates the [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md) to remove the download with the provided [downloadId](-remove-download-action/download-id.md). |
| [UpdateDownloadAction](-update-download-action/index.md) | `data class UpdateDownloadAction : `[`DownloadAction`](./index.md)<br>Updates the provided [download](-update-download-action/download.md) on the [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md). |
