[android-components](../../index.md) / [mozilla.components.browser.toolbar.edit](../index.md) / [EditToolbar](./index.md)

# EditToolbar

`class EditToolbar` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/toolbar/src/main/java/mozilla/components/browser/toolbar/edit/EditToolbar.kt#L53)

Sub-component of the browser toolbar responsible for allowing the user to edit the URL ("edit mode").

Structure:
+------+---------------------------+---------+------+
| icon |           url             | actions | exit |
+------+---------------------------+---------+------+

* icon: Optional icon that will be shown in front of the URL.
* url: Editable URL of the currently displayed website
* actions: Optional action icons injected by other components (e.g. barcode scanner)
* exit: Button that switches back to display mode or invoke an app-defined callback.

### Types

| Name | Summary |
|---|---|
| [Colors](-colors/index.md) | `data class Colors`<br>Data class holding the customizable colors in "edit mode". |

### Properties

| Name | Summary |
|---|---|
| [colors](colors.md) | `var colors: `[`Colors`](-colors/index.md)<br>Customizable colors in "edit mode". |
| [hint](hint.md) | `var hint: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Sets the text to be displayed when the URL of the toolbar is empty. |
| [textSize](text-size.md) | `var textSize: `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html)<br>Sets the size of the text for the URL/search term displayed in the toolbar. |
| [typeface](typeface.md) | `var typeface: <ERROR CLASS>`<br>Sets the typeface of the text for the URL/search term displayed in the toolbar. |

### Functions

| Name | Summary |
|---|---|
| [focus](focus.md) | `fun focus(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Focuses the url input field and shows the virtual keyboard if needed. |
| [setIcon](set-icon.md) | `fun setIcon(icon: <ERROR CLASS>, contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets an icon that will be drawn in front of the URL. |
| [setOnEditFocusChangeListener](set-on-edit-focus-change-listener.md) | `fun setOnEditFocusChangeListener(listener: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets a listener to be invoked when focus of the URL input view (in edit mode) changed. |
| [setUrlBackground](set-url-background.md) | `fun setUrlBackground(background: <ERROR CLASS>?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the background that will be drawn behind the URL, icon and edit actions. |
| [updateUrl](update-url.md) | `fun updateUrl(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, shouldAutoComplete: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, shouldHighlight: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Updates the text of the URL input field. Note: this does *not* affect the value of url itself and is only a visual change |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
