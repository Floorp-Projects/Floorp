[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [DownloadAction](../index.md) / [RemoveDownloadAction](./index.md)

# RemoveDownloadAction

`data class RemoveDownloadAction : `[`DownloadAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L719)

Updates the [BrowserState](../../../mozilla.components.browser.state.state/-browser-state/index.md) to remove the download with the provided [downloadId](download-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `RemoveDownloadAction(downloadId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>Updates the [BrowserState](../../../mozilla.components.browser.state.state/-browser-state/index.md) to remove the download with the provided [downloadId](download-id.md). |

### Properties

| Name | Summary |
|---|---|
| [downloadId](download-id.md) | `val downloadId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
