[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [EngineSession](index.md) / [goToHistoryIndex](./go-to-history-index.md)

# goToHistoryIndex

`abstract fun goToHistoryIndex(index: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSession.kt#L504)

Navigates to the specified index in the [HistoryState](#) of this session. The current index of
this session's [HistoryState](#) will be updated but the items within it will be unchanged.
Invalid index values are ignored.

### Parameters

`index` - the index of the session's [HistoryState](#) to navigate to