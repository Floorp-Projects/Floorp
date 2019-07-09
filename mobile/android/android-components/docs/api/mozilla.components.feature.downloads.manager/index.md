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
| [OnDownloadCompleted](-on-download-completed.md) | `typealias OnDownloadCompleted = (`[`Download`](../mozilla.components.browser.session/-download/index.md)`, `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [SystemDownloadManager](-system-download-manager.md) | `typealias SystemDownloadManager = <ERROR CLASS>` |
| [SystemRequest](-system-request.md) | `typealias SystemRequest = <ERROR CLASS>` |

### Functions

| Name | Summary |
|---|---|
| [getFileName](get-file-name.md) | `fun `[`Download`](../mozilla.components.browser.session/-download/index.md)`.getFileName(contentDisposition: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [isScheme](is-scheme.md) | `fun `[`Download`](../mozilla.components.browser.session/-download/index.md)`.isScheme(protocols: `[`Iterable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-iterable/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [validatePermissionGranted](validate-permission-granted.md) | `fun `[`DownloadManager`](-download-manager/index.md)`.validatePermissionGranted(context: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
