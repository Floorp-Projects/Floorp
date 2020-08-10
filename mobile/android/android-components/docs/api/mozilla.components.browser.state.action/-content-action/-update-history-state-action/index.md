[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [ContentAction](../index.md) / [UpdateHistoryStateAction](./index.md)

# UpdateHistoryStateAction

`data class UpdateHistoryStateAction : `[`ContentAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L304)

Updates the [ContentState](../../../mozilla.components.browser.state.state/-content-state/index.md) of the given [sessionId](session-id.md) to indicate the current history state.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UpdateHistoryStateAction(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, historyList: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`HistoryItem`](../../../mozilla.components.concept.engine.history/-history-item/index.md)`>, currentIndex: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`)`<br>Updates the [ContentState](../../../mozilla.components.browser.state.state/-content-state/index.md) of the given [sessionId](session-id.md) to indicate the current history state. |

### Properties

| Name | Summary |
|---|---|
| [currentIndex](current-index.md) | `val currentIndex: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [historyList](history-list.md) | `val historyList: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`HistoryItem`](../../../mozilla.components.concept.engine.history/-history-item/index.md)`>` |
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
