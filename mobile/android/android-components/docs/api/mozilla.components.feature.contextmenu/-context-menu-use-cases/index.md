[android-components](../../index.md) / [mozilla.components.feature.contextmenu](../index.md) / [ContextMenuUseCases](./index.md)

# ContextMenuUseCases

`class ContextMenuUseCases` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/contextmenu/src/main/java/mozilla/components/feature/contextmenu/ContextMenuUseCases.kt#L19)

Contains use cases related to the context menu feature.

### Parameters

`sessionManager` - the application's [SessionManager](../../mozilla.components.browser.session/-session-manager/index.md).

### Types

| Name | Summary |
|---|---|
| [ConsumeHitResultUseCase](-consume-hit-result-use-case/index.md) | `class ConsumeHitResultUseCase` |
| [InjectDownloadUseCase](-inject-download-use-case/index.md) | `class InjectDownloadUseCase` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ContextMenuUseCases(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`)`<br>Contains use cases related to the context menu feature. |

### Properties

| Name | Summary |
|---|---|
| [consumeHitResult](consume-hit-result.md) | `val consumeHitResult: `[`ConsumeHitResultUseCase`](-consume-hit-result-use-case/index.md) |
| [injectDownload](inject-download.md) | `val injectDownload: `[`InjectDownloadUseCase`](-inject-download-use-case/index.md) |
