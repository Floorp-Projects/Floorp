[android-components](../index.md) / [mozilla.components.feature.downloads.manager](./index.md)

## Package mozilla.components.feature.downloads.manager

### Types

| Name | Summary |
|---|---|
| [AndroidDownloadManager](-android-download-manager/index.md) | `class AndroidDownloadManager : `[`DownloadManager`](-download-manager/index.md)<br>Handles the interactions with the [AndroidDownloadManager](-android-download-manager/index.md). |
| [DownloadManager](-download-manager/index.md) | `interface DownloadManager` |
| [FetchDownloadManager](-fetch-download-manager/index.md) | `class FetchDownloadManager<T : `[`AbstractFetchDownloadService`](../mozilla.components.feature.downloads/-abstract-fetch-download-service/index.md)`> : `[`DownloadManager`](-download-manager/index.md)<br>Handles the interactions with [AbstractFetchDownloadService](../mozilla.components.feature.downloads/-abstract-fetch-download-service/index.md). |

### Type Aliases

| Name | Summary |
|---|---|
| [SystemDownloadManager](-system-download-manager.md) | `typealias SystemDownloadManager = <ERROR CLASS>` |
| [SystemRequest](-system-request.md) | `typealias SystemRequest = <ERROR CLASS>` |
| [onDownloadStopped](on-download-stopped.md) | `typealias onDownloadStopped = (`[`DownloadState`](../mozilla.components.browser.state.state.content/-download-state/index.md)`, `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, `[`Status`](../mozilla.components.browser.state.state.content/-download-state/-status/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Functions

| Name | Summary |
|---|---|
| [validatePermissionGranted](validate-permission-granted.md) | `fun `[`DownloadManager`](-download-manager/index.md)`.validatePermissionGranted(context: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
