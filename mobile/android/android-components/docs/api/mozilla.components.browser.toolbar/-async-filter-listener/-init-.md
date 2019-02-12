[android-components](../../index.md) / [mozilla.components.browser.toolbar](../index.md) / [AsyncFilterListener](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`AsyncFilterListener(urlView: `[`AutocompleteView`](../../mozilla.components.ui.autocomplete/-autocomplete-view/index.md)`, coroutineContext: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)`, filter: suspend (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`AutocompleteDelegate`](../../mozilla.components.concept.toolbar/-autocomplete-delegate/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, uiContext: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = Dispatchers.Main)`

Wraps [filter](-init-.md#mozilla.components.browser.toolbar.AsyncFilterListener$<init>(mozilla.components.ui.autocomplete.AutocompleteView, kotlin.coroutines.CoroutineContext, kotlin.SuspendFunction2((kotlin.String, mozilla.components.concept.toolbar.AutocompleteDelegate, kotlin.Unit)), kotlin.coroutines.CoroutineContext)/filter) execution in a coroutine context, cancelling prior executions on every invocation.
[coroutineContext](-init-.md#mozilla.components.browser.toolbar.AsyncFilterListener$<init>(mozilla.components.ui.autocomplete.AutocompleteView, kotlin.coroutines.CoroutineContext, kotlin.SuspendFunction2((kotlin.String, mozilla.components.concept.toolbar.AutocompleteDelegate, kotlin.Unit)), kotlin.coroutines.CoroutineContext)/coroutineContext) must be of type that doesn't propagate cancellation of its children upwards.

