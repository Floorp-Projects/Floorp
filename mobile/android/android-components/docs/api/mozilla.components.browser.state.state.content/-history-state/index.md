[android-components](../../index.md) / [mozilla.components.browser.state.state.content](../index.md) / [HistoryState](./index.md)

# HistoryState

`data class HistoryState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/content/HistoryState.kt#L17)

Value type that represents browser history.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `HistoryState(items: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`HistoryItem`](../../mozilla.components.concept.engine.history/-history-item/index.md)`> = emptyList(), currentIndex: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0)`<br>Value type that represents browser history. |

### Properties

| Name | Summary |
|---|---|
| [currentIndex](current-index.md) | `val currentIndex: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>The index of the currently selected [HistoryItem](../../mozilla.components.concept.engine.history/-history-item/index.md). If this is equal to lastIndex, then there are no pages to go "forward" to. If this is 0, then there are no pages to go "back" to. |
| [items](items.md) | `val items: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`HistoryItem`](../../mozilla.components.concept.engine.history/-history-item/index.md)`>`<br>All the items in the browser history. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
