[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [MediaAction](../index.md) / [UpdateMediaFullscreenAction](./index.md)

# UpdateMediaFullscreenAction

`data class UpdateMediaFullscreenAction : `[`MediaAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L556)

Updates the [Media.fullscreen](../../../mozilla.components.concept.engine.media/-media/fullscreen.md) for the [MediaState.Element](../../../mozilla.components.browser.state.state/-media-state/-element/index.md) with id [mediaId](media-id.md) owned by the tab
with id [tabId](tab-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UpdateMediaFullscreenAction(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, mediaId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, fullScreen: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`)`<br>Updates the [Media.fullscreen](../../../mozilla.components.concept.engine.media/-media/fullscreen.md) for the [MediaState.Element](../../../mozilla.components.browser.state.state/-media-state/-element/index.md) with id [mediaId](media-id.md) owned by the tab with id [tabId](tab-id.md). |

### Properties

| Name | Summary |
|---|---|
| [fullScreen](full-screen.md) | `val fullScreen: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [mediaId](media-id.md) | `val mediaId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [tabId](tab-id.md) | `val tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
