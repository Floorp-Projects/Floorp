[android-components](../../../index.md) / [mozilla.components.concept.toolbar](../../index.md) / [Toolbar](../index.md) / [ActionButton](./index.md)

# ActionButton

`open class ActionButton : `[`Action`](../-action/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/toolbar/src/main/java/mozilla/components/concept/toolbar/Toolbar.kt#L186)

An action button to be added to the toolbar.

### Parameters

`imageDrawable` - The drawable to be shown.

`contentDescription` - The content description to use.

`visible` - Lambda that returns true or false to indicate whether this button should be shown.

`padding` - A optional custom padding.

`listener` - Callback that will be invoked whenever the button is pressed

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ActionButton(imageDrawable: <ERROR CLASS>? = null, contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true }, background: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0, padding: `[`Padding`](../../../mozilla.components.support.base.android/-padding/index.md)`? = null, listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>An action button to be added to the toolbar. |

### Properties

| Name | Summary |
|---|---|
| [contentDescription](content-description.md) | `val contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The content description to use. |
| [imageDrawable](image-drawable.md) | `val imageDrawable: <ERROR CLASS>?`<br>The drawable to be shown. |
| [visible](visible.md) | `open val visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Lambda that returns true or false to indicate whether this button should be shown. |

### Functions

| Name | Summary |
|---|---|
| [bind](bind.md) | `open fun bind(view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [createView](create-view.md) | `open fun createView(parent: <ERROR CLASS>): <ERROR CLASS>` |

### Inheritors

| Name | Summary |
|---|---|
| [Button](../../../mozilla.components.browser.toolbar/-browser-toolbar/-button/index.md) | `class Button : `[`ActionButton`](./index.md)<br>An action button to be added to the toolbar. |
