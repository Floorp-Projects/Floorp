[android-components](../../index.md) / [mozilla.components.feature.session](../index.md) / [HistoryDelegate](index.md) / [getVisited](./get-visited.md)

# getVisited

`suspend fun getVisited(uris: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/HistoryDelegate.kt#L32)

Overrides [HistoryTrackingDelegate.getVisited](../../mozilla.components.concept.engine.history/-history-tracking-delegate/get-visited.md)

An engine needs to know "visited" (true/false) status for provided URIs.

`suspend fun getVisited(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/HistoryDelegate.kt#L36)

Overrides [HistoryTrackingDelegate.getVisited](../../mozilla.components.concept.engine.history/-history-tracking-delegate/get-visited.md)

An engine needs to know a list of all visited URIs.

