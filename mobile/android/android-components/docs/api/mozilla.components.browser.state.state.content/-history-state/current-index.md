[android-components](../../index.md) / [mozilla.components.browser.state.state.content](../index.md) / [HistoryState](index.md) / [currentIndex](./current-index.md)

# currentIndex

`val currentIndex: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/content/HistoryState.kt#L19)

The index of the currently selected [HistoryItem](../../mozilla.components.concept.engine.history/-history-item/index.md).
If this is equal to lastIndex, then there are no pages to go "forward" to.
If this is 0, then there are no pages to go "back" to.

### Property

`currentIndex` - The index of the currently selected [HistoryItem](../../mozilla.components.concept.engine.history/-history-item/index.md).
If this is equal to lastIndex, then there are no pages to go "forward" to.
If this is 0, then there are no pages to go "back" to.