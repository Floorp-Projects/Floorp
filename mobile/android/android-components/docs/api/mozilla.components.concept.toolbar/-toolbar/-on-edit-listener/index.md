[android-components](../../../index.md) / [mozilla.components.concept.toolbar](../../index.md) / [Toolbar](../index.md) / [OnEditListener](./index.md)

# OnEditListener

`interface OnEditListener` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/toolbar/src/main/java/mozilla/components/concept/toolbar/Toolbar.kt#L143)

Listener to be invoked when the user edits the URL.

### Functions

| Name | Summary |
|---|---|
| [onCancelEditing](on-cancel-editing.md) | `open fun onCancelEditing(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Fired when the user presses the back button while in edit mode. |
| [onStartEditing](on-start-editing.md) | `open fun onStartEditing(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Fired when the toolbar switches to edit mode. |
| [onStopEditing](on-stop-editing.md) | `open fun onStopEditing(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Fired when the toolbar switches back to display mode. |
| [onTextChanged](on-text-changed.md) | `open fun onTextChanged(text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Fired whenever the user changes the text in the address bar. |
