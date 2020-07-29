[android-components](../../index.md) / [mozilla.components.feature.downloads.manager](../index.md) / [FetchDownloadManager](index.md) / [tryAgain](./try-again.md)

# tryAgain

`fun tryAgain(downloadId: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/downloads/src/main/java/mozilla/components/feature/downloads/manager/FetchDownloadManager.kt#L70)

Overrides [DownloadManager.tryAgain](../-download-manager/try-again.md)

Schedules another attempt at downloading the given download.

### Parameters

`downloadId` - the id of the previously attempted download