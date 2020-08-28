[android-components](../../index.md) / [mozilla.components.feature.downloads](../index.md) / [DownloadStorage](./index.md)

# DownloadStorage

`class DownloadStorage` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/downloads/src/main/java/mozilla/components/feature/downloads/DownloadStorage.kt#L19)

A storage implementation for organizing download.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DownloadStorage(context: <ERROR CLASS>)`<br>A storage implementation for organizing download. |

### Functions

| Name | Summary |
|---|---|
| [add](add.md) | `suspend fun add(download: `[`DownloadState`](../../mozilla.components.browser.state.state.content/-download-state/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds a new [download](add.md#mozilla.components.feature.downloads.DownloadStorage$add(mozilla.components.browser.state.state.content.DownloadState)/download). |
| [getDownloads](get-downloads.md) | `fun getDownloads(): Flow<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`DownloadState`](../../mozilla.components.browser.state.state.content/-download-state/index.md)`>>`<br>Returns a [Flow](#) list of all the [DownloadState](../../mozilla.components.browser.state.state.content/-download-state/index.md) instances. |
| [getDownloadsPaged](get-downloads-paged.md) | `fun getDownloadsPaged(): Factory<`[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, `[`DownloadState`](../../mozilla.components.browser.state.state.content/-download-state/index.md)`>`<br>Returns all saved [DownloadState](../../mozilla.components.browser.state.state.content/-download-state/index.md) instances as a [DataSource.Factory](#). |
| [remove](remove.md) | `suspend fun remove(download: `[`DownloadState`](../../mozilla.components.browser.state.state.content/-download-state/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes the given [download](remove.md#mozilla.components.feature.downloads.DownloadStorage$remove(mozilla.components.browser.state.state.content.DownloadState)/download). |
| [removeAllDownloads](remove-all-downloads.md) | `suspend fun removeAllDownloads(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes all the downloads. |
| [update](update.md) | `suspend fun update(download: `[`DownloadState`](../../mozilla.components.browser.state.state.content/-download-state/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Update the given [download](update.md#mozilla.components.feature.downloads.DownloadStorage$update(mozilla.components.browser.state.state.content.DownloadState)/download). |

### Companion Object Functions

| Name | Summary |
|---|---|
| [isSameDownload](is-same-download.md) | `fun isSameDownload(first: `[`DownloadState`](../../mozilla.components.browser.state.state.content/-download-state/index.md)`, second: `[`DownloadState`](../../mozilla.components.browser.state.state.content/-download-state/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Takes two [DownloadState](../../mozilla.components.browser.state.state.content/-download-state/index.md) objects and the determine if they are the same, be aware this only takes into considerations fields that are being stored, not all the field on [DownloadState](../../mozilla.components.browser.state.state.content/-download-state/index.md) are stored. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
