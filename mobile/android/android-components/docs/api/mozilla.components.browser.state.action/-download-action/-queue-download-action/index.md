[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [DownloadAction](../index.md) / [QueueDownloadAction](./index.md)

# QueueDownloadAction

`data class QueueDownloadAction : `[`DownloadAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L569)

Updates the [BrowserState](../../../mozilla.components.browser.state.state/-browser-state/index.md) to track the provided [download](download.md) as queued.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `QueueDownloadAction(download: `[`DownloadState`](../../../mozilla.components.browser.state.state.content/-download-state/index.md)`)`<br>Updates the [BrowserState](../../../mozilla.components.browser.state.state/-browser-state/index.md) to track the provided [download](download.md) as queued. |

### Properties

| Name | Summary |
|---|---|
| [download](download.md) | `val download: `[`DownloadState`](../../../mozilla.components.browser.state.state.content/-download-state/index.md) |
