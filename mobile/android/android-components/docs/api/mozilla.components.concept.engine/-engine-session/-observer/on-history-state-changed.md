[android-components](../../../index.md) / [mozilla.components.concept.engine](../../index.md) / [EngineSession](../index.md) / [Observer](index.md) / [onHistoryStateChanged](./on-history-state-changed.md)

# onHistoryStateChanged

`open fun onHistoryStateChanged(historyList: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`HistoryItem`](../../../mozilla.components.concept.engine.history/-history-item/index.md)`>, currentIndex: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSession.kt#L133)

Event to indicate that this session has changed its history state.

### Parameters

`historyList` - The list of items in the session history.

`currentIndex` - Index of the current page in the history list.