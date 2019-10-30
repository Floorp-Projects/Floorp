[android-components](../../index.md) / [mozilla.components.feature.downloads](../index.md) / [DownloadsUseCases](./index.md)

# DownloadsUseCases

`class DownloadsUseCases` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/downloads/src/main/java/mozilla/components/feature/downloads/DownloadsUseCases.kt#L15)

Contains use cases related to the downloads feature.

### Parameters

`store` - the application's [BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md).

### Types

| Name | Summary |
|---|---|
| [ConsumeDownloadUseCase](-consume-download-use-case/index.md) | `class ConsumeDownloadUseCase` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DownloadsUseCases(store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`)`<br>Contains use cases related to the downloads feature. |

### Properties

| Name | Summary |
|---|---|
| [consumeDownload](consume-download.md) | `val consumeDownload: `[`ConsumeDownloadUseCase`](-consume-download-use-case/index.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
