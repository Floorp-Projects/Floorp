[android-components](../../index.md) / [mozilla.components.browser.state.action](../index.md) / [DownloadAction](./index.md)

# DownloadAction

`sealed class DownloadAction : `[`BrowserAction`](../-browser-action.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L570)

[BrowserAction](../-browser-action.md) implementations related to updating the global download state.

### Types

| Name | Summary |
|---|---|
| [QueueDownloadAction](-queue-download-action/index.md) | `data class QueueDownloadAction : `[`DownloadAction`](./index.md)<br>Updates the [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md) to track the provided [download](-queue-download-action/download.md) as queued. |
| [RemoveAllQueuedDownloadsAction](-remove-all-queued-downloads-action.md) | `object RemoveAllQueuedDownloadsAction : `[`DownloadAction`](./index.md)<br>Updates the [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md) to remove all queued downloads. |
| [RemoveQueuedDownloadAction](-remove-queued-download-action/index.md) | `data class RemoveQueuedDownloadAction : `[`DownloadAction`](./index.md)<br>Updates the [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md) to remove the queued download with the provided [downloadId](-remove-queued-download-action/download-id.md). |
| [UpdateQueuedDownloadAction](-update-queued-download-action/index.md) | `data class UpdateQueuedDownloadAction : `[`DownloadAction`](./index.md)<br>Updates the provided [download](-update-queued-download-action/download.md) on the [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [QueueDownloadAction](-queue-download-action/index.md) | `data class QueueDownloadAction : `[`DownloadAction`](./index.md)<br>Updates the [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md) to track the provided [download](-queue-download-action/download.md) as queued. |
| [RemoveAllQueuedDownloadsAction](-remove-all-queued-downloads-action.md) | `object RemoveAllQueuedDownloadsAction : `[`DownloadAction`](./index.md)<br>Updates the [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md) to remove all queued downloads. |
| [RemoveQueuedDownloadAction](-remove-queued-download-action/index.md) | `data class RemoveQueuedDownloadAction : `[`DownloadAction`](./index.md)<br>Updates the [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md) to remove the queued download with the provided [downloadId](-remove-queued-download-action/download-id.md). |
| [UpdateQueuedDownloadAction](-update-queued-download-action/index.md) | `data class UpdateQueuedDownloadAction : `[`DownloadAction`](./index.md)<br>Updates the provided [download](-update-queued-download-action/download.md) on the [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md). |
