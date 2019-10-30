[android-components](../../index.md) / [mozilla.components.concept.engine.history](../index.md) / [HistoryTrackingDelegate](./index.md)

# HistoryTrackingDelegate

`interface HistoryTrackingDelegate` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/history/HistoryTrackingDelegate.kt#L16)

An interface used for providing history information to an engine (e.g. for link highlighting),
and receiving history updates from the engine (visits to URLs, title changes).

Even though this interface is defined at the "concept" layer, its get* methods are tailored to
two types of engines which we support (system's WebView and GeckoView).

### Functions

| Name | Summary |
|---|---|
| [getVisited](get-visited.md) | `abstract suspend fun getVisited(uris: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>An engine needs to know "visited" (true/false) status for provided URIs.`abstract suspend fun getVisited(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>An engine needs to know a list of all visited URIs. |
| [onTitleChanged](on-title-changed.md) | `abstract suspend fun onTitleChanged(uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Title changed for a given URI. |
| [onVisited](on-visited.md) | `abstract suspend fun onVisited(uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, visit: `[`PageVisit`](../../mozilla.components.concept.storage/-page-visit/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A URI visit happened that an engine considers worthy of being recorded in browser's history. |
| [shouldStoreUri](should-store-uri.md) | `abstract fun shouldStoreUri(uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Allows an engine to check if this URI is going to be accepted by the delegate. This helps avoid unnecessary coroutine overhead for URIs which won't be accepted. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [HistoryDelegate](../../mozilla.components.feature.session/-history-delegate/index.md) | `class HistoryDelegate : `[`HistoryTrackingDelegate`](./index.md)<br>Implementation of the [HistoryTrackingDelegate](./index.md) which delegates work to an instance of [HistoryStorage](../../mozilla.components.concept.storage/-history-storage/index.md). |
