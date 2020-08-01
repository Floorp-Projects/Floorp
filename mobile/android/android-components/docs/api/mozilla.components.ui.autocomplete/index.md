[android-components](../index.md) / [mozilla.components.ui.autocomplete](./index.md)

## Package mozilla.components.ui.autocomplete

### Types

| Name | Summary |
|---|---|
| [AutocompleteView](-autocomplete-view/index.md) | `interface AutocompleteView`<br>Aids in testing functionality which relies on some aspects of InlineAutocompleteEditText. |
| [InlineAutocompleteEditText](-inline-autocomplete-edit-text/index.md) | `open class InlineAutocompleteEditText : AppCompatEditText, `[`AutocompleteView`](-autocomplete-view/index.md)<br>A UI edit text component which supports inline autocompletion. |

### Type Aliases

| Name | Summary |
|---|---|
| [OnCommitListener](-on-commit-listener.md) | `typealias OnCommitListener = () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [OnDispatchKeyEventPreImeListener](-on-dispatch-key-event-pre-ime-listener.md) | `typealias OnDispatchKeyEventPreImeListener = (<ERROR CLASS>?) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [OnFilterListener](-on-filter-listener.md) | `typealias OnFilterListener = (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [OnKeyPreImeListener](-on-key-pre-ime-listener.md) | `typealias OnKeyPreImeListener = (<ERROR CLASS>, `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, <ERROR CLASS>) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [OnSearchStateChangeListener](-on-search-state-change-listener.md) | `typealias OnSearchStateChangeListener = (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [OnSelectionChangedListener](-on-selection-changed-listener.md) | `typealias OnSelectionChangedListener = (`[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [OnTextChangeListener](-on-text-change-listener.md) | `typealias OnTextChangeListener = (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [OnWindowsFocusChangeListener](-on-windows-focus-change-listener.md) | `typealias OnWindowsFocusChangeListener = (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [TextFormatter](-text-formatter.md) | `typealias TextFormatter = (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
