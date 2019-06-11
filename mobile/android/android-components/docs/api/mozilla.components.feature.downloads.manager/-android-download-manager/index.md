[android-components](../../index.md) / [mozilla.components.feature.downloads.manager](../index.md) / [AndroidDownloadManager](./index.md)

# AndroidDownloadManager

`class AndroidDownloadManager : `[`DownloadManager`](../-download-manager/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/downloads/src/main/java/mozilla/components/feature/downloads/manager/AndroidDownloadManager.kt#L39)

Handles the interactions with the [AndroidDownloadManager](./index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AndroidDownloadManager(applicationContext: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, onDownloadCompleted: `[`OnDownloadCompleted`](../-on-download-completed.md)` = { _, _ -> })`<br>Handles the interactions with the [AndroidDownloadManager](./index.md). |

### Properties

| Name | Summary |
|---|---|
| [onDownloadCompleted](on-download-completed.md) | `var onDownloadCompleted: `[`OnDownloadCompleted`](../-on-download-completed.md)<br>a callback to be notified when a download is completed. |

### Functions

| Name | Summary |
|---|---|
| [download](download.md) | `fun download(download: `[`Download`](../../mozilla.components.browser.session/-download/index.md)`, refererUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, cookie: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>Schedule a download through the [AndroidDownloadManager](./index.md). |
| [unregisterListener](unregister-listener.md) | `fun unregisterListener(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Remove all the listeners. |
