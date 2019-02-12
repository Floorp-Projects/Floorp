[android-components](../index.md) / [mozilla.components.ui.autocomplete](index.md) / [OnFilterListener](./-on-filter-listener.md)

# OnFilterListener

`typealias OnFilterListener = (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/ui/autocomplete/src/main/java/mozilla/components/ui/autocomplete/InlineAutocompleteEditText.kt#L35)

### Inheritors

| Name | Summary |
|---|---|
| [AsyncFilterListener](../mozilla.components.browser.toolbar/-async-filter-listener/index.md) | `class AsyncFilterListener : `[`OnFilterListener`](./-on-filter-listener.md)`, CoroutineScope`<br>Wraps [filter](#) execution in a coroutine context, cancelling prior executions on every invocation. [coroutineContext](../mozilla.components.browser.toolbar/-async-filter-listener/coroutine-context.md) must be of type that doesn't propagate cancellation of its children upwards. |
