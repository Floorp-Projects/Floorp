[android-components](../../../index.md) / [mozilla.components.feature.contextmenu](../../index.md) / [ContextMenuUseCases](../index.md) / [InjectDownloadUseCase](./index.md)

# InjectDownloadUseCase

`class InjectDownloadUseCase` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/contextmenu/src/main/java/mozilla/components/feature/contextmenu/ContextMenuUseCases.kt#L31)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `InjectDownloadUseCase(store: `[`BrowserStore`](../../../mozilla.components.browser.state.store/-browser-store/index.md)`)` |

### Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `operator fun invoke(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, download: `[`DownloadState`](../../../mozilla.components.browser.state.state.content/-download-state/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds a [DownloadState](../../../mozilla.components.browser.state.state.content/-download-state/index.md) to the [BrowserStore](../../../mozilla.components.browser.state.store/-browser-store/index.md) with the given [tabId](invoke.md#mozilla.components.feature.contextmenu.ContextMenuUseCases.InjectDownloadUseCase$invoke(kotlin.String, mozilla.components.browser.state.state.content.DownloadState)/tabId). |
