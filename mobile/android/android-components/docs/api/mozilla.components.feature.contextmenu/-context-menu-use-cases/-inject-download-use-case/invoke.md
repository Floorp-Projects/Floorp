[android-components](../../../index.md) / [mozilla.components.feature.contextmenu](../../index.md) / [ContextMenuUseCases](../index.md) / [InjectDownloadUseCase](index.md) / [invoke](./invoke.md)

# invoke

`operator fun invoke(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, download: `[`DownloadState`](../../../mozilla.components.browser.state.state.content/-download-state/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/contextmenu/src/main/java/mozilla/components/feature/contextmenu/ContextMenuUseCases.kt#L45)

Adds a [Download](#) to the [Session](../../../mozilla.components.browser.session/-session/index.md) with the given [tabId](invoke.md#mozilla.components.feature.contextmenu.ContextMenuUseCases.InjectDownloadUseCase$invoke(kotlin.String, mozilla.components.browser.state.state.content.DownloadState)/tabId).

This is a hacky workaround. After we have migrated everything from browser-session to
browser-state we should revisits this and find a better solution.

