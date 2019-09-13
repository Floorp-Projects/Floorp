[android-components](../../index.md) / [mozilla.components.feature.downloads.manager](../index.md) / [DownloadManager](./index.md)

# DownloadManager

`interface DownloadManager` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/downloads/src/main/java/mozilla/components/feature/downloads/manager/DownloadManager.kt#L13)

### Properties

| Name | Summary |
|---|---|
| [onDownloadCompleted](on-download-completed.md) | `abstract var onDownloadCompleted: `[`OnDownloadCompleted`](../-on-download-completed.md) |
| [permissions](permissions.md) | `abstract val permissions: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` |

### Functions

| Name | Summary |
|---|---|
| [download](download.md) | `abstract fun download(download: `[`DownloadState`](../../mozilla.components.browser.state.state.content/-download-state/index.md)`, cookie: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = ""): `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`?`<br>Schedules a download through the [DownloadManager](./index.md). |
| [unregisterListeners](unregister-listeners.md) | `open fun unregisterListeners(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [validatePermissionGranted](../validate-permission-granted.md) | `fun `[`DownloadManager`](./index.md)`.validatePermissionGranted(context: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [AndroidDownloadManager](../-android-download-manager/index.md) | `class AndroidDownloadManager : `[`DownloadManager`](./index.md)<br>Handles the interactions with the [AndroidDownloadManager](../-android-download-manager/index.md). |
| [FetchDownloadManager](../-fetch-download-manager/index.md) | `class FetchDownloadManager<T : `[`AbstractFetchDownloadService`](../../mozilla.components.feature.downloads/-abstract-fetch-download-service/index.md)`> : `[`DownloadManager`](./index.md)<br>Handles the interactions with [AbstractFetchDownloadService](../../mozilla.components.feature.downloads/-abstract-fetch-download-service/index.md). |
