[android-components](../../index.md) / [mozilla.components.feature.downloads.manager](../index.md) / [FetchDownloadManager](./index.md)

# FetchDownloadManager

`class FetchDownloadManager<T : `[`AbstractFetchDownloadService`](../../mozilla.components.feature.downloads/-abstract-fetch-download-service/index.md)`> : `[`DownloadManager`](../-download-manager/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/downloads/src/main/java/mozilla/components/feature/downloads/manager/FetchDownloadManager.kt#L35)

Handles the interactions with [AbstractFetchDownloadService](../../mozilla.components.feature.downloads/-abstract-fetch-download-service/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FetchDownloadManager(applicationContext: <ERROR CLASS>, service: `[`KClass`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.reflect/-k-class/index.html)`<`[`T`](index.md#T)`>, broadcastManager: LocalBroadcastManager = LocalBroadcastManager.getInstance(applicationContext), onDownloadCompleted: `[`OnDownloadCompleted`](../-on-download-completed.md)` = noop)`<br>Handles the interactions with [AbstractFetchDownloadService](../../mozilla.components.feature.downloads/-abstract-fetch-download-service/index.md). |

### Properties

| Name | Summary |
|---|---|
| [onDownloadCompleted](on-download-completed.md) | `var onDownloadCompleted: `[`OnDownloadCompleted`](../-on-download-completed.md) |
| [permissions](permissions.md) | `val permissions: <ERROR CLASS>` |

### Functions

| Name | Summary |
|---|---|
| [download](download.md) | `fun download(download: `[`DownloadState`](../../mozilla.components.browser.state.state.content/-download-state/index.md)`, cookie: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`?`<br>Schedules a download through the [AbstractFetchDownloadService](../../mozilla.components.feature.downloads/-abstract-fetch-download-service/index.md). |
| [onReceive](on-receive.md) | `fun onReceive(context: <ERROR CLASS>, intent: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked when a download is complete. Calls [onDownloadCompleted](on-download-completed.md) and unregisters the broadcast receiver if there are no more queued downloads. |
| [unregisterListeners](unregister-listeners.md) | `fun unregisterListeners(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Remove all the listeners. |

### Extension Functions

| Name | Summary |
|---|---|
| [validatePermissionGranted](../validate-permission-granted.md) | `fun `[`DownloadManager`](../-download-manager/index.md)`.validatePermissionGranted(context: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
