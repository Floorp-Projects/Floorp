[android-components](../../index.md) / [mozilla.components.feature.downloads](../index.md) / [DownloadStorage](index.md) / [isSameDownload](./is-same-download.md)

# isSameDownload

`fun isSameDownload(first: `[`DownloadState`](../../mozilla.components.browser.state.state.content/-download-state/index.md)`, second: `[`DownloadState`](../../mozilla.components.browser.state.state.content/-download-state/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/downloads/src/main/java/mozilla/components/feature/downloads/DownloadStorage.kt#L78)

Takes two [DownloadState](../../mozilla.components.browser.state.state.content/-download-state/index.md) objects and the determine if they are the same, be aware this
only takes into considerations fields that are being stored,
not all the field on [DownloadState](../../mozilla.components.browser.state.state.content/-download-state/index.md) are stored.

