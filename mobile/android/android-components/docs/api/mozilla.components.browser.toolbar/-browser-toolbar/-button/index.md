[android-components](../../../index.md) / [mozilla.components.browser.toolbar](../../index.md) / [BrowserToolbar](../index.md) / [Button](./index.md)

# Button

`class Button : `[`ActionButton`](../../../mozilla.components.concept.toolbar/-toolbar/-action-button/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/toolbar/src/main/java/mozilla/components/browser/toolbar/BrowserToolbar.kt#L704)

An action button to be added to the toolbar.

### Parameters

`imageDrawable` - The drawable to be shown.

`contentDescription` - The content description to use.

`visible` - Lambda that returns true or false to indicate whether this button should be shown.

`background` - A custom (stateful) background drawable resource to be used.

`padding` - a custom [Padding](../../../mozilla.components.support.base.android/-padding/index.md) for this Button.

`listener` - Callback that will be invoked whenever the button is pressed

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Button(imageDrawable: <ERROR CLASS>, contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true }, background: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0, padding: `[`Padding`](../../../mozilla.components.support.base.android/-padding/index.md)` = DEFAULT_PADDING, listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>An action button to be added to the toolbar. |

### Properties

| Name | Summary |
|---|---|
| [padding](padding.md) | `val padding: `[`Padding`](../../../mozilla.components.support.base.android/-padding/index.md)<br>a custom [Padding](../../../mozilla.components.support.base.android/-padding/index.md) for this Button. |

### Inherited Properties

| Name | Summary |
|---|---|
| [contentDescription](../../../mozilla.components.concept.toolbar/-toolbar/-action-button/content-description.md) | `val contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The content description to use. |
| [imageDrawable](../../../mozilla.components.concept.toolbar/-toolbar/-action-button/image-drawable.md) | `val imageDrawable: <ERROR CLASS>?`<br>The drawable to be shown. |
| [visible](../../../mozilla.components.concept.toolbar/-toolbar/-action-button/visible.md) | `open val visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Lambda that returns true or false to indicate whether this button should be shown. |

### Inherited Functions

| Name | Summary |
|---|---|
| [bind](../../../mozilla.components.concept.toolbar/-toolbar/-action-button/bind.md) | `open fun bind(view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [createView](../../../mozilla.components.concept.toolbar/-toolbar/-action-button/create-view.md) | `open fun createView(parent: <ERROR CLASS>): <ERROR CLASS>` |

### Inheritors

| Name | Summary |
|---|---|
| [TwoStateButton](../-two-state-button/index.md) | `class TwoStateButton : `[`Button`](./index.md)<br>An action that either shows an active button or an inactive button based on the provided isEnabled lambda. |
