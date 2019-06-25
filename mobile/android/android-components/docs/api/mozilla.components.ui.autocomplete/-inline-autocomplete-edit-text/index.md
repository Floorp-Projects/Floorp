[android-components](../../index.md) / [mozilla.components.ui.autocomplete](../index.md) / [InlineAutocompleteEditText](./index.md)

# InlineAutocompleteEditText

`open class InlineAutocompleteEditText : AppCompatEditText, `[`AutocompleteView`](../-autocomplete-view/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/ui/autocomplete/src/main/java/mozilla/components/ui/autocomplete/InlineAutocompleteEditText.kt#L88)

A UI edit text component which supports inline autocompletion.

The background color of autocomplete spans can be configured using
the custom autocompleteBackgroundColor attribute e.g.
app:autocompleteBackgroundColor="#ffffff".

A filter listener (see [setOnFilterListener](set-on-filter-listener.md)) needs to be attached to
provide autocomplete results. It will be invoked when the input
text changes. The listener gets direct access to this component (via its view
parameter), so it can call {@link applyAutocompleteResult} in return.

A commit listener (see [setOnCommitListener](set-on-commit-listener.md)) can be attached which is
invoked when the user selected the result i.e. is done editing.

Various other listeners can be attached to enhance default behaviour e.g.
[setOnSelectionChangedListener](set-on-selection-changed-listener.md) and [setOnWindowsFocusChangeListener](set-on-windows-focus-change-listener.md) which
will be invoked in response to [onSelectionChanged](on-selection-changed.md) and [onWindowFocusChanged](on-window-focus-changed.md)
respectively (see also [setOnTextChangeListener](set-on-text-change-listener.md),
[setOnSelectionChangedListener](set-on-selection-changed-listener.md), and [setOnWindowsFocusChangeListener](set-on-windows-focus-change-listener.md)).

### Types

| Name | Summary |
|---|---|
| [AutocompleteResult](-autocomplete-result/index.md) | `data class AutocompleteResult` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `InlineAutocompleteEditText(ctx: <ERROR CLASS>, attrs: <ERROR CLASS>? = null, defStyleAttr: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = R.attr.editTextStyle)`<br>A UI edit text component which supports inline autocompletion. |

### Properties

| Name | Summary |
|---|---|
| [autoCompleteBackgroundColor](auto-complete-background-color.md) | `var autoCompleteBackgroundColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>The background color used for the autocomplete suggestion. |
| [autoCompleteForegroundColor](auto-complete-foreground-color.md) | `var autoCompleteForegroundColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?`<br>The Foreground color used for the autocomplete suggestion. |
| [autocompleteResult](autocomplete-result.md) | `var autocompleteResult: `[`AutocompleteResult`](-autocomplete-result/index.md)`?` |
| [nonAutocompleteText](non-autocomplete-text.md) | `val nonAutocompleteText: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [originalText](original-text.md) | `open val originalText: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Current text. |

### Functions

| Name | Summary |
|---|---|
| [applyAutocompleteResult](apply-autocomplete-result.md) | `open fun applyAutocompleteResult(result: `[`AutocompleteResult`](-autocomplete-result/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Applies the provided result by updating the current autocomplete text and selection, if any. |
| [dispatchKeyEventPreIme](dispatch-key-event-pre-ime.md) | `open fun dispatchKeyEventPreIme(event: <ERROR CLASS>?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [getText](get-text.md) | `open fun getText(): <ERROR CLASS>` |
| [noAutocompleteResult](no-autocomplete-result.md) | `open fun noAutocompleteResult(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notify that there is no autocomplete result available. |
| [onAttachedToWindow](on-attached-to-window.md) | `open fun onAttachedToWindow(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onCreateInputConnection](on-create-input-connection.md) | `open fun onCreateInputConnection(outAttrs: <ERROR CLASS>): <ERROR CLASS>?`<br>Code to handle deleting autocomplete first when backspacing. If there is no autocomplete text, both removeAutocomplete() and commitAutocomplete() are no-ops and return false. Therefore we can use them here without checking explicitly if we have autocomplete text or not. |
| [onFocusChanged](on-focus-changed.md) | `open fun onFocusChanged(gainFocus: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, direction: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, previouslyFocusedRect: <ERROR CLASS>?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onKeyPreIme](on-key-pre-ime.md) | `open fun onKeyPreIme(keyCode: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, event: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onSelectionChanged](on-selection-changed.md) | `open fun onSelectionChanged(selStart: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, selEnd: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onTouchEvent](on-touch-event.md) | `open fun onTouchEvent(event: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onWindowFocusChanged](on-window-focus-changed.md) | `open fun onWindowFocusChanged(hasFocus: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [sendAccessibilityEventUnchecked](send-accessibility-event-unchecked.md) | `open fun sendAccessibilityEventUnchecked(event: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setOnCommitListener](set-on-commit-listener.md) | `fun setOnCommitListener(l: `[`OnCommitListener`](../-on-commit-listener.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setOnDispatchKeyEventPreImeListener](set-on-dispatch-key-event-pre-ime-listener.md) | `fun setOnDispatchKeyEventPreImeListener(l: `[`OnDispatchKeyEventPreImeListener`](../-on-dispatch-key-event-pre-ime-listener.md)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setOnFilterListener](set-on-filter-listener.md) | `fun setOnFilterListener(l: `[`OnFilterListener`](../-on-filter-listener.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setOnKeyPreImeListener](set-on-key-pre-ime-listener.md) | `fun setOnKeyPreImeListener(l: `[`OnKeyPreImeListener`](../-on-key-pre-ime-listener.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setOnSearchStateChangeListener](set-on-search-state-change-listener.md) | `fun setOnSearchStateChangeListener(l: `[`OnSearchStateChangeListener`](../-on-search-state-change-listener.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setOnSelectionChangedListener](set-on-selection-changed-listener.md) | `fun setOnSelectionChangedListener(l: `[`OnSelectionChangedListener`](../-on-selection-changed-listener.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setOnTextChangeListener](set-on-text-change-listener.md) | `fun setOnTextChangeListener(l: `[`OnTextChangeListener`](../-on-text-change-listener.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setOnWindowsFocusChangeListener](set-on-windows-focus-change-listener.md) | `fun setOnWindowsFocusChangeListener(l: `[`OnWindowsFocusChangeListener`](../-on-windows-focus-change-listener.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setText](set-text.md) | `open fun setText(text: `[`CharSequence`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char-sequence/index.html)`?, type: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>`fun setText(text: `[`CharSequence`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char-sequence/index.html)`?, shouldAutoComplete: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Companion Object Properties

| Name | Summary |
|---|---|
| [INPUT_METHOD_AMAZON_ECHO_SHOW](-i-n-p-u-t_-m-e-t-h-o-d_-a-m-a-z-o-n_-e-c-h-o_-s-h-o-w.md) | `const val INPUT_METHOD_AMAZON_ECHO_SHOW: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [INPUT_METHOD_SONY](-i-n-p-u-t_-m-e-t-h-o-d_-s-o-n-y.md) | `const val INPUT_METHOD_SONY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
