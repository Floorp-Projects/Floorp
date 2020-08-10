[android-components](../../index.md) / [mozilla.components.feature.search](../index.md) / [SearchFeature](./index.md)

# SearchFeature

`class SearchFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/search/src/main/java/mozilla/components/feature/search/SearchFeature.kt#L28)

Lifecycle aware feature that forwards [SearchRequest](../../mozilla.components.concept.engine.search/-search-request/index.md)s from the [store](#) to [performSearch](#), and
then consumes them.

NOTE: that this only consumes SearchRequests, and will not hook up the [store](#) to a source of
SearchRequests. See [SelectionActionDelegate](#) for use in generating SearchRequests.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SearchFeature(store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, performSearch: (`[`SearchRequest`](../../mozilla.components.concept.engine.search/-search-request/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>Lifecycle aware feature that forwards [SearchRequest](../../mozilla.components.concept.engine.search/-search-request/index.md)s from the [store](#) to [performSearch](#), and then consumes them. |

### Functions

| Name | Summary |
|---|---|
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
