[android-components](../../index.md) / [mozilla.components.feature.session](../index.md) / [HistoryDelegate](index.md) / [onVisited](./on-visited.md)

# onVisited

`suspend fun onVisited(uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, visit: `[`PageVisit`](../../mozilla.components.concept.storage/-page-visit/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/HistoryDelegate.kt#L17)

Overrides [HistoryTrackingDelegate.onVisited](../../mozilla.components.concept.engine.history/-history-tracking-delegate/on-visited.md)

A URI visit happened that an engine considers worthy of being recorded in browser's history.

