[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [ContentAction](../index.md) / [UpdateDownloadAction](./index.md)

# UpdateDownloadAction

`data class UpdateDownloadAction : `[`ContentAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L180)

Updates the [DownloadState](../../../mozilla.components.browser.state.state.content/-download-state/index.md) of the [ContentState](../../../mozilla.components.browser.state.state/-content-state/index.md) with the given [sessionId](session-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UpdateDownloadAction(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, download: `[`DownloadState`](../../../mozilla.components.browser.state.state.content/-download-state/index.md)`)`<br>Updates the [DownloadState](../../../mozilla.components.browser.state.state.content/-download-state/index.md) of the [ContentState](../../../mozilla.components.browser.state.state/-content-state/index.md) with the given [sessionId](session-id.md). |

### Properties

| Name | Summary |
|---|---|
| [download](download.md) | `val download: `[`DownloadState`](../../../mozilla.components.browser.state.state.content/-download-state/index.md) |
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
