[android-components](../../../index.md) / [mozilla.components.browser.menu.item](../../index.md) / [BrowserMenuItemToolbar](../index.md) / [Button](./index.md)

# Button

`class Button` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/item/BrowserMenuItemToolbar.kt#L70)

A button to be shown in a toolbar inside the browser menu.

### Parameters

`imageResource` - ID of a drawable resource to be shown as icon.

`contentDescription` - The button's content description, used for accessibility support.

`iconTintColorResource` - Optional ID of color resource to tint the icon.

`isEnabled` - Lambda to return true/false to indicate if this button should be enabled or disabled.

`listener` - Callback to be invoked when the button is pressed.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Button(imageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, iconTintColorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, isEnabled: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true }, listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>A button to be shown in a toolbar inside the browser menu. |

### Properties

| Name | Summary |
|---|---|
| [contentDescription](content-description.md) | `val contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The button's content description, used for accessibility support. |
| [iconTintColorResource](icon-tint-color-resource.md) | `val iconTintColorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Optional ID of color resource to tint the icon. |
| [imageResource](image-resource.md) | `val imageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>ID of a drawable resource to be shown as icon. |
| [isEnabled](is-enabled.md) | `val isEnabled: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Lambda to return true/false to indicate if this button should be enabled or disabled. |
| [listener](listener.md) | `val listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Callback to be invoked when the button is pressed. |

### Inheritors

| Name | Summary |
|---|---|
| [TwoStateButton](../-two-state-button/index.md) | `class TwoStateButton : `[`Button`](./index.md)<br>A button that either shows an primary state or an secondary state based on the provided isInPrimaryState lambda. |
