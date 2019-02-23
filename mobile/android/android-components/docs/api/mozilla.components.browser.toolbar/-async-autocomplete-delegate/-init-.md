[android-components](../../index.md) / [mozilla.components.browser.toolbar](../index.md) / [AsyncAutocompleteDelegate](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`AsyncAutocompleteDelegate(urlView: `[`AutocompleteView`](../../mozilla.components.ui.autocomplete/-autocomplete-view/index.md)`, parentScope: CoroutineScope, coroutineContext: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)`, logger: `[`Logger`](../../mozilla.components.support.base.log.logger/-logger/index.md)` = Logger("AsyncAutocompleteDelegate"))`

An autocomplete delegate which is aware of its parent scope (to check for cancellations).
Responsible for processing autocompletion results and discarding stale results when [urlView](#) moved on.

