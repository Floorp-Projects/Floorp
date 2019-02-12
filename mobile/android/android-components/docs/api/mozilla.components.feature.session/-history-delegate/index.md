[android-components](../../index.md) / [mozilla.components.feature.session](../index.md) / [HistoryDelegate](./index.md)

# HistoryDelegate

`class HistoryDelegate : `[`HistoryTrackingDelegate`](../../mozilla.components.concept.engine.history/-history-tracking-delegate/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/HistoryDelegate.kt#L15)

Implementation of the [HistoryTrackingDelegate](../../mozilla.components.concept.engine.history/-history-tracking-delegate/index.md) which delegates work to an instance of [HistoryStorage](../../mozilla.components.concept.storage/-history-storage/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `HistoryDelegate(historyStorage: `[`HistoryStorage`](../../mozilla.components.concept.storage/-history-storage/index.md)`)`<br>Implementation of the [HistoryTrackingDelegate](../../mozilla.components.concept.engine.history/-history-tracking-delegate/index.md) which delegates work to an instance of [HistoryStorage](../../mozilla.components.concept.storage/-history-storage/index.md). |

### Functions

| Name | Summary |
|---|---|
| [getVisited](get-visited.md) | `suspend fun getVisited(uris: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>An engine needs to know "visited" (true/false) status for provided URIs.`suspend fun getVisited(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>An engine needs to know a list of all visited URIs. |
| [onTitleChanged](on-title-changed.md) | `suspend fun onTitleChanged(uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Title changed for a given URI. |
| [onVisited](on-visited.md) | `suspend fun onVisited(uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, isReload: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A URI visit happened that an engine considers worthy of being recorded in browser's history. |
