[android-components](../../index.md) / [mozilla.components.feature.contextmenu](../index.md) / [ContextMenuUseCases](./index.md)

# ContextMenuUseCases

`class ContextMenuUseCases` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/contextmenu/src/main/java/mozilla/components/feature/contextmenu/ContextMenuUseCases.kt#L17)

Contains use cases related to the context menu feature.

### Parameters

`store` - the application's [BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md).

### Types

| Name | Summary |
|---|---|
| [ConsumeHitResultUseCase](-consume-hit-result-use-case/index.md) | `class ConsumeHitResultUseCase` |
| [InjectDownloadUseCase](-inject-download-use-case/index.md) | `class InjectDownloadUseCase` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ContextMenuUseCases(store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`)`<br>Contains use cases related to the context menu feature. |

### Properties

| Name | Summary |
|---|---|
| [consumeHitResult](consume-hit-result.md) | `val consumeHitResult: `[`ConsumeHitResultUseCase`](-consume-hit-result-use-case/index.md) |
| [injectDownload](inject-download.md) | `val injectDownload: `[`InjectDownloadUseCase`](-inject-download-use-case/index.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
