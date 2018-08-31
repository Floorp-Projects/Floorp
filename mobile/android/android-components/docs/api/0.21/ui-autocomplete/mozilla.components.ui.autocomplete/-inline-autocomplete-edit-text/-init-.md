---
title: InlineAutocompleteEditText.<init> - 
---

[mozilla.components.ui.autocomplete](../index.html) / [InlineAutocompleteEditText](index.html) / [&lt;init&gt;](./-init-.html)

# &lt;init&gt;

`InlineAutocompleteEditText(ctx: Context, attrs: AttributeSet? = null, defStyleAttr: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = R.attr.editTextStyle)`

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

