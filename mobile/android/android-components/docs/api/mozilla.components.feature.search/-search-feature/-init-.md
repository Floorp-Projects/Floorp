[android-components](../../index.md) / [mozilla.components.feature.search](../index.md) / [SearchFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`SearchFeature(store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, performSearch: (`[`SearchRequest`](../../mozilla.components.concept.engine.search/-search-request/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`

Lifecycle aware feature that forwards [SearchRequest](../../mozilla.components.concept.engine.search/-search-request/index.md)s from the [store](#) to [performSearch](#), and
then consumes them.

NOTE: that this only consumes SearchRequests, and will not hook up the [store](#) to a source of
SearchRequests. See [SelectionActionDelegate](#) for use in generating SearchRequests.

