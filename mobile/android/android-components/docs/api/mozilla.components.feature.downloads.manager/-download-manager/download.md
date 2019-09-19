[android-components](../../index.md) / [mozilla.components.feature.downloads.manager](../index.md) / [DownloadManager](index.md) / [download](./download.md)

# download

`abstract fun download(download: `[`DownloadState`](../../mozilla.components.browser.state.state.content/-download-state/index.md)`, cookie: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = ""): `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/downloads/src/main/java/mozilla/components/feature/downloads/manager/DownloadManager.kt#L25)

Schedules a download through the [DownloadManager](index.md).

### Parameters

`download` - metadata related to the download.

`cookie` - any additional cookie to add as part of the download request.

**Return**
the id reference of the scheduled download.

