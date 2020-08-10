[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [MediaAction](../index.md) / [AddMediaAction](./index.md)

# AddMediaAction

`data class AddMediaAction : `[`MediaAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L500)

Adds [media](media.md) to the list of [MediaState.Element](../../../mozilla.components.browser.state.state/-media-state/-element/index.md) for the tab with id [tabId](tab-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AddMediaAction(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, media: `[`Element`](../../../mozilla.components.browser.state.state/-media-state/-element/index.md)`)`<br>Adds [media](media.md) to the list of [MediaState.Element](../../../mozilla.components.browser.state.state/-media-state/-element/index.md) for the tab with id [tabId](tab-id.md). |

### Properties

| Name | Summary |
|---|---|
| [media](media.md) | `val media: `[`Element`](../../../mozilla.components.browser.state.state/-media-state/-element/index.md) |
| [tabId](tab-id.md) | `val tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
