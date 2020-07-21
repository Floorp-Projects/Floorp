[android-components](../../index.md) / [mozilla.components.feature.session](../index.md) / [HistoryDelegate](./index.md)

# HistoryDelegate

`class HistoryDelegate : `[`HistoryTrackingDelegate`](../../mozilla.components.concept.engine.history/-history-tracking-delegate/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/HistoryDelegate.kt#L16)

Implementation of the [HistoryTrackingDelegate](../../mozilla.components.concept.engine.history/-history-tracking-delegate/index.md) which delegates work to an instance of [HistoryStorage](../../mozilla.components.concept.storage/-history-storage/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `HistoryDelegate(historyStorage: `[`Lazy`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-lazy/index.html)`<`[`HistoryStorage`](../../mozilla.components.concept.storage/-history-storage/index.md)`>)`<br>Implementation of the [HistoryTrackingDelegate](../../mozilla.components.concept.engine.history/-history-tracking-delegate/index.md) which delegates work to an instance of [HistoryStorage](../../mozilla.components.concept.storage/-history-storage/index.md). |

### Functions

| Name | Summary |
|---|---|
| [getVisited](get-visited.md) | `suspend fun getVisited(uris: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>An engine needs to know "visited" (true/false) status for provided URIs.`suspend fun getVisited(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>An engine needs to know a list of all visited URIs. |
| [onTitleChanged](on-title-changed.md) | `suspend fun onTitleChanged(uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Title changed for a given URI. |
| [onVisited](on-visited.md) | `suspend fun onVisited(uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, visit: `[`PageVisit`](../../mozilla.components.concept.storage/-page-visit/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A URI visit happened that an engine considers worthy of being recorded in browser's history. |
| [shouldStoreUri](should-store-uri.md) | `fun shouldStoreUri(uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Filter out unwanted URIs, such as "chrome:", "about:", etc. Ported from nsAndroidHistory::CanAddURI See https://dxr.mozilla.org/mozilla-central/source/mobile/android/components/build/nsAndroidHistory.cpp#326 |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
