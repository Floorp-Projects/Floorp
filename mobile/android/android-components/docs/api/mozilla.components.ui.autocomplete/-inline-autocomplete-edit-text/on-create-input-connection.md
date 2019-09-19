[android-components](../../index.md) / [mozilla.components.ui.autocomplete](../index.md) / [InlineAutocompleteEditText](index.md) / [onCreateInputConnection](./on-create-input-connection.md)

# onCreateInputConnection

`open fun onCreateInputConnection(outAttrs: <ERROR CLASS>): <ERROR CLASS>?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/ui/autocomplete/src/main/java/mozilla/components/ui/autocomplete/InlineAutocompleteEditText.kt#L534)

Code to handle deleting autocomplete first when backspacing.
If there is no autocomplete text, both removeAutocomplete() and commitAutocomplete()
are no-ops and return false. Therefore we can use them here without checking explicitly
if we have autocomplete text or not.

Also turns off text prediction for private mode tabs.

