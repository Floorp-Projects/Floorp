[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [MediaAction](../index.md) / [RemoveTabMediaAction](./index.md)

# RemoveTabMediaAction

`data class RemoveTabMediaAction : `[`MediaAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L510)

Removes all [MediaState.Element](../../../mozilla.components.browser.state.state/-media-state/-element/index.md) for tabs with ids in [tabIds](tab-ids.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `RemoveTabMediaAction(tabIds: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>)`<br>Removes all [MediaState.Element](../../../mozilla.components.browser.state.state/-media-state/-element/index.md) for tabs with ids in [tabIds](tab-ids.md). |

### Properties

| Name | Summary |
|---|---|
| [tabIds](tab-ids.md) | `val tabIds: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` |
