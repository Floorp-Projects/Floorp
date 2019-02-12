[android-components](../../index.md) / [mozilla.components.concept.engine.history](../index.md) / [HistoryTrackingDelegate](index.md) / [onVisited](./on-visited.md)

# onVisited

`abstract suspend fun onVisited(uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, isReload: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/history/HistoryTrackingDelegate.kt#L18)

A URI visit happened that an engine considers worthy of being recorded in browser's history.

