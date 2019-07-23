[android-components](../../index.md) / [mozilla.components.feature.downloads](../index.md) / [DownloadDialogFragment](index.md) / [onStartDownload](./on-start-download.md)

# onStartDownload

`var onStartDownload: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/downloads/src/main/java/mozilla/components/feature/downloads/DownloadDialogFragment.kt#L25)

A callback to trigger a download, call it when you are ready to start a download. For instance,
a valid use case can be in confirmation dialog, after the positive button is clicked,
this callback must be called.

