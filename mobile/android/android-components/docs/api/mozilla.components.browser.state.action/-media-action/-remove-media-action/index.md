[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [MediaAction](../index.md) / [RemoveMediaAction](./index.md)

# RemoveMediaAction

`data class RemoveMediaAction : `[`MediaAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L505)

Removes [media](media.md) from the list of [MediaState.Element](../../../mozilla.components.browser.state.state/-media-state/-element/index.md) for the tab with id [tabId](tab-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `RemoveMediaAction(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, media: `[`Element`](../../../mozilla.components.browser.state.state/-media-state/-element/index.md)`)`<br>Removes [media](media.md) from the list of [MediaState.Element](../../../mozilla.components.browser.state.state/-media-state/-element/index.md) for the tab with id [tabId](tab-id.md). |

### Properties

| Name | Summary |
|---|---|
| [media](media.md) | `val media: `[`Element`](../../../mozilla.components.browser.state.state/-media-state/-element/index.md) |
| [tabId](tab-id.md) | `val tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
