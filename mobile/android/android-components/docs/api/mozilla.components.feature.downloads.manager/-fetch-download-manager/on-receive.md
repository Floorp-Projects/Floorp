[android-components](../../index.md) / [mozilla.components.feature.downloads.manager](../index.md) / [FetchDownloadManager](index.md) / [onReceive](./on-receive.md)

# onReceive

`fun onReceive(context: <ERROR CLASS>, intent: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/downloads/src/main/java/mozilla/components/feature/downloads/manager/FetchDownloadManager.kt#L103)

Invoked when a download is complete. Calls [onDownloadCompleted](on-download-completed.md) and unregisters the
broadcast receiver if there are no more queued downloads.

