[android-components](../../../index.md) / [mozilla.components.feature.downloads](../../index.md) / [DownloadsUseCases](../index.md) / [ConsumeDownloadUseCase](./index.md)

# ConsumeDownloadUseCase

`class ConsumeDownloadUseCase` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/downloads/src/main/java/mozilla/components/feature/downloads/DownloadsUseCases.kt#L18)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ConsumeDownloadUseCase(store: `[`BrowserStore`](../../../mozilla.components.browser.state.store/-browser-store/index.md)`)` |

### Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `operator fun invoke(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, downloadId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Consumes the download with the given [downloadId](invoke.md#mozilla.components.feature.downloads.DownloadsUseCases.ConsumeDownloadUseCase$invoke(kotlin.String, kotlin.String)/downloadId) from the session with the given [tabId](invoke.md#mozilla.components.feature.downloads.DownloadsUseCases.ConsumeDownloadUseCase$invoke(kotlin.String, kotlin.String)/tabId). |
