[android-components](../../index.md) / [mozilla.components.feature.downloads.manager](../index.md) / [DownloadManager](./index.md)

# DownloadManager

`interface DownloadManager` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/downloads/src/main/java/mozilla/components/feature/downloads/manager/DownloadManager.kt#L14)

### Properties

| Name | Summary |
|---|---|
| [onDownloadStopped](on-download-stopped.md) | `abstract var onDownloadStopped: `[`onDownloadStopped`](../on-download-stopped.md) |
| [permissions](permissions.md) | `abstract val permissions: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` |

### Functions

| Name | Summary |
|---|---|
| [download](download.md) | `abstract fun download(download: `[`DownloadState`](../../mozilla.components.browser.state.state.content/-download-state/index.md)`, cookie: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = ""): `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`?`<br>Schedules a download through the [DownloadManager](./index.md). |
| [tryAgain](try-again.md) | `abstract fun tryAgain(downloadId: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Schedules another attempt at downloading the given download. |
| [unregisterListeners](unregister-listeners.md) | `open fun unregisterListeners(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
| [validatePermissionGranted](../validate-permission-granted.md) | `fun `[`DownloadManager`](./index.md)`.validatePermissionGranted(context: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [AndroidDownloadManager](../-android-download-manager/index.md) | `class AndroidDownloadManager : `[`DownloadManager`](./index.md)<br>Handles the interactions with the [AndroidDownloadManager](../-android-download-manager/index.md). |
| [FetchDownloadManager](../-fetch-download-manager/index.md) | `class FetchDownloadManager<T : `[`AbstractFetchDownloadService`](../../mozilla.components.feature.downloads/-abstract-fetch-download-service/index.md)`> : `[`DownloadManager`](./index.md)<br>Handles the interactions with [AbstractFetchDownloadService](../../mozilla.components.feature.downloads/-abstract-fetch-download-service/index.md). |
