[android-components](../index.md) / [mozilla.components.browser.toolbar](./index.md)

## Package mozilla.components.browser.toolbar

### Types

| Name | Summary |
|---|---|
| [AsyncAutocompleteDelegate](-async-autocomplete-delegate/index.md) | `class AsyncAutocompleteDelegate : `[`AutocompleteDelegate`](../mozilla.components.concept.toolbar/-autocomplete-delegate/index.md)`, CoroutineScope`<br>An autocomplete delegate which is aware of its parent scope (to check for cancellations). Responsible for processing autocompletion results and discarding stale results when [urlView](#) moved on. |
| [AsyncFilterListener](-async-filter-listener/index.md) | `class AsyncFilterListener : `[`OnFilterListener`](../mozilla.components.ui.autocomplete/-on-filter-listener.md)`, CoroutineScope`<br>Wraps [filter](#) execution in a coroutine context, cancelling prior executions on every invocation. [coroutineContext](-async-filter-listener/coroutine-context.md) must be of type that doesn't propagate cancellation of its children upwards. |
| [BrowserToolbar](-browser-toolbar/index.md) | `class BrowserToolbar : `[`Toolbar`](../mozilla.components.concept.toolbar/-toolbar/index.md)<br>A customizable toolbar for browsers. |
