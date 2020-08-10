[android-components](../../index.md) / [mozilla.components.concept.engine.history](../index.md) / [HistoryTrackingDelegate](index.md) / [getVisited](./get-visited.md)

# getVisited

`abstract suspend fun getVisited(uris: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/history/HistoryTrackingDelegate.kt#L30)

An engine needs to know "visited" (true/false) status for provided URIs.

`abstract suspend fun getVisited(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/history/HistoryTrackingDelegate.kt#L35)

An engine needs to know a list of all visited URIs.

