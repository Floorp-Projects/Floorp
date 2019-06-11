[android-components](../../index.md) / [mozilla.components.feature.downloads.manager](../index.md) / [DownloadManager](./index.md)

# DownloadManager

`interface DownloadManager` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/downloads/src/main/java/mozilla/components/feature/downloads/manager/DownloadManager.kt#L13)

### Properties

| Name | Summary |
|---|---|
| [onDownloadCompleted](on-download-completed.md) | `abstract var onDownloadCompleted: `[`OnDownloadCompleted`](../-on-download-completed.md) |

### Functions

| Name | Summary |
|---|---|
| [download](download.md) | `abstract fun download(download: `[`Download`](../../mozilla.components.browser.session/-download/index.md)`, refererUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", cookie: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = ""): `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |
| [unregisterListener](unregister-listener.md) | `abstract fun unregisterListener(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [AndroidDownloadManager](../-android-download-manager/index.md) | `class AndroidDownloadManager : `[`DownloadManager`](./index.md)<br>Handles the interactions with the [AndroidDownloadManager](../-android-download-manager/index.md). |
