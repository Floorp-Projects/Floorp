[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [MediaAction](../index.md) / [UpdateMediaVolumeAction](./index.md)

# UpdateMediaVolumeAction

`data class UpdateMediaVolumeAction : `[`MediaAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L546)

Updates the [Media.Volume](../../../mozilla.components.concept.engine.media/-media/-volume/index.md) for the [MediaState.Element](../../../mozilla.components.browser.state.state/-media-state/-element/index.md) with id [mediaId](media-id.md) owned by the tab
with id [tabId](tab-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UpdateMediaVolumeAction(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, mediaId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, volume: `[`Volume`](../../../mozilla.components.concept.engine.media/-media/-volume/index.md)`)`<br>Updates the [Media.Volume](../../../mozilla.components.concept.engine.media/-media/-volume/index.md) for the [MediaState.Element](../../../mozilla.components.browser.state.state/-media-state/-element/index.md) with id [mediaId](media-id.md) owned by the tab with id [tabId](tab-id.md). |

### Properties

| Name | Summary |
|---|---|
| [mediaId](media-id.md) | `val mediaId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [tabId](tab-id.md) | `val tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [volume](volume.md) | `val volume: `[`Volume`](../../../mozilla.components.concept.engine.media/-media/-volume/index.md) |
