---
title: InlineAutocompleteEditText - 
---

[mozilla.components.ui.autocomplete](../index.html) / [InlineAutocompleteEditText](./index.html)

# InlineAutocompleteEditText

`open class InlineAutocompleteEditText : AppCompatEditText`

A UI edit text component which supports inline autocompletion.

The background color of autocomplete spans can be configured using
the custom autocompleteBackgroundColor attribute e.g.
app:autocompleteBackgroundColor="#ffffff".

A filter listener (see [setOnFilterListener](set-on-filter-listener.html)) needs to be attached to
provide autocomplete results. It will be invoked when the input
text changes. The listener gets direct access to this component (via its view
parameter), so it can call {@link applyAutocompleteResult} in return.

A commit listener (see [setOnCommitListener](set-on-commit-listener.html)) can be attached which is
invoked when the user selected the result i.e. is done editing.

Various other listeners can be attached to enhance default behaviour e.g.
[setOnSelectionChangedListener](set-on-selection-changed-listener.html) and [setOnWindowsFocusChangeListener](set-on-windows-focus-change-listener.html) which
will be invoked in response to [onSelectionChanged](on-selection-changed.html) and [onWindowFocusChanged](on-window-focus-changed.html)
respectively (see also [setOnTextChangeListener](set-on-text-change-listener.html),
[setOnSelectionChangedListener](set-on-selection-changed-listener.html), and [setOnWindowsFocusChangeListener](set-on-windows-focus-change-listener.html)).

### Types

| [AutocompleteResult](-autocomplete-result/index.html) | `data class AutocompleteResult` |

### Constructors

| [&lt;init&gt;](-init-.html) | `InlineAutocompleteEditText(ctx: Context, attrs: AttributeSet? = null, defStyleAttr: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = R.attr.editTextStyle)`<br>A UI edit text component which supports inline autocompletion. |

### Properties

| [autocompleteResult](autocomplete-result.html) | `var autocompleteResult: `[`AutocompleteResult`](-autocomplete-result/index.html) |
| [ctx](ctx.html) | `val ctx: Context` |
| [nonAutocompleteText](non-autocomplete-text.html) | `val nonAutocompleteText: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [originalText](original-text.html) | `val originalText: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Functions

| [applyAutocompleteResult](apply-autocomplete-result.html) | `fun applyAutocompleteResult(result: `[`AutocompleteResult`](-autocomplete-result/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Applies the provided result by updating the current autocomplete text and selection, if any. |
| [onAttachedToWindow](on-attached-to-window.html) | `open fun onAttachedToWindow(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onCreateInputConnection](on-create-input-connection.html) | `open fun onCreateInputConnection(outAttrs: EditorInfo): InputConnection?`<br>Code to handle deleting autocomplete first when backspacing. If there is no autocomplete text, both removeAutocomplete() and commitAutocomplete() are no-ops and return false. Therefore we can use them here without checking explicitly if we have autocomplete text or not. |
| [onFocusChanged](on-focus-changed.html) | `open fun onFocusChanged(gainFocus: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, direction: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, previouslyFocusedRect: Rect?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onKeyPreIme](on-key-pre-ime.html) | `open fun onKeyPreIme(keyCode: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, event: KeyEvent): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onSelectionChanged](on-selection-changed.html) | `open fun onSelectionChanged(selStart: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, selEnd: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onTouchEvent](on-touch-event.html) | `open fun onTouchEvent(event: MotionEvent): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onWindowFocusChanged](on-window-focus-changed.html) | `open fun onWindowFocusChanged(hasFocus: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [sendAccessibilityEventUnchecked](send-accessibility-event-unchecked.html) | `open fun sendAccessibilityEventUnchecked(event: AccessibilityEvent): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setOnCommitListener](set-on-commit-listener.html) | `fun setOnCommitListener(l: `[`OnCommitListener`](../-on-commit-listener.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setOnFilterListener](set-on-filter-listener.html) | `fun setOnFilterListener(l: `[`OnFilterListener`](../-on-filter-listener.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setOnKeyPreImeListener](set-on-key-pre-ime-listener.html) | `fun setOnKeyPreImeListener(l: `[`OnKeyPreImeListener`](../-on-key-pre-ime-listener.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setOnSearchStateChangeListener](set-on-search-state-change-listener.html) | `fun setOnSearchStateChangeListener(l: `[`OnSearchStateChangeListener`](../-on-search-state-change-listener.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setOnSelectionChangedListener](set-on-selection-changed-listener.html) | `fun setOnSelectionChangedListener(l: `[`OnSelectionChangedListener`](../-on-selection-changed-listener.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setOnTextChangeListener](set-on-text-change-listener.html) | `fun setOnTextChangeListener(l: `[`OnTextChangeListener`](../-on-text-change-listener.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setOnWindowsFocusChangeListener](set-on-windows-focus-change-listener.html) | `fun setOnWindowsFocusChangeListener(l: `[`OnWindowsFocusChangeListener`](../-on-windows-focus-change-listener.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setText](set-text.html) | `open fun setText(text: `[`CharSequence`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char-sequence/index.html)`?, type: BufferType): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Companion Object Properties

| [AUTOCOMPLETE_SPAN](-a-u-t-o-c-o-m-p-l-e-t-e_-s-p-a-n.html) | `val AUTOCOMPLETE_SPAN: Concrete` |
| [DEFAULT_AUTOCOMPLETE_BACKGROUND_COLOR](-d-e-f-a-u-l-t_-a-u-t-o-c-o-m-p-l-e-t-e_-b-a-c-k-g-r-o-u-n-d_-c-o-l-o-r.html) | `val DEFAULT_AUTOCOMPLETE_BACKGROUND_COLOR: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

