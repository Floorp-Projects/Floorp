---
title: InlineAutocompleteEditText.onCreateInputConnection - 
---

[mozilla.components.ui.autocomplete](../index.html) / [InlineAutocompleteEditText](index.html) / [onCreateInputConnection](./on-create-input-connection.html)

# onCreateInputConnection

`open fun onCreateInputConnection(outAttrs: EditorInfo): InputConnection?`

Code to handle deleting autocomplete first when backspacing.
If there is no autocomplete text, both removeAutocomplete() and commitAutocomplete()
are no-ops and return false. Therefore we can use them here without checking explicitly
if we have autocomplete text or not.

Also turns off text prediction for private mode tabs.

