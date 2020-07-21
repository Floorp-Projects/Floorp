[android-components](../../index.md) / [mozilla.components.ui.autocomplete](../index.md) / [InlineAutocompleteEditText](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`InlineAutocompleteEditText(ctx: <ERROR CLASS>, attrs: <ERROR CLASS>? = null, defStyleAttr: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = R.attr.editTextStyle)`

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

