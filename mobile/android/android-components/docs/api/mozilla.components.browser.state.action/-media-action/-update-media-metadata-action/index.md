[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [MediaAction](../index.md) / [UpdateMediaMetadataAction](./index.md)

# UpdateMediaMetadataAction

`data class UpdateMediaMetadataAction : `[`MediaAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L536)

Updates the [Media.Metadata](../../../mozilla.components.concept.engine.media/-media/-metadata/index.md) for the [MediaState.Element](../../../mozilla.components.browser.state.state/-media-state/-element/index.md) with id [mediaId](media-id.md) owned by the tab
with id [tabId](tab-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UpdateMediaMetadataAction(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, mediaId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, metadata: `[`Metadata`](../../../mozilla.components.concept.engine.media/-media/-metadata/index.md)`)`<br>Updates the [Media.Metadata](../../../mozilla.components.concept.engine.media/-media/-metadata/index.md) for the [MediaState.Element](../../../mozilla.components.browser.state.state/-media-state/-element/index.md) with id [mediaId](media-id.md) owned by the tab with id [tabId](tab-id.md). |

### Properties

| Name | Summary |
|---|---|
| [mediaId](media-id.md) | `val mediaId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [metadata](metadata.md) | `val metadata: `[`Metadata`](../../../mozilla.components.concept.engine.media/-media/-metadata/index.md) |
| [tabId](tab-id.md) | `val tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
