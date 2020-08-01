[android-components](../../../index.md) / [mozilla.components.browser.menu.item](../../index.md) / [BrowserMenuItemToolbar](../index.md) / [TwoStateButton](./index.md)

# TwoStateButton

`class TwoStateButton : `[`Button`](../-button/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/item/BrowserMenuItemToolbar.kt#L147)

A button that either shows an primary state or an secondary state based on the provided
isInPrimaryState lambda.

### Parameters

`primaryImageResource` - ID of a drawable resource to be shown as primary icon.

`primaryContentDescription` - The button's primary content description, used for accessibility support.

`primaryImageTintResource` - Optional ID of color resource to tint the primary icon.

`secondaryImageResource` - Optional ID of a different drawable resource to be shown as secondary icon.

`secondaryContentDescription` - Optional secondary content description for button, for accessibility support.

`secondaryImageTintResource` - Optional ID of secondary color resource to tint the icon.

`isInPrimaryState` - Lambda to return true/false to indicate if this button should be primary or secondary.

`disableInSecondaryState` - Optional boolean to disable the button when in secondary state.

`longClickListener` - Callback to be invoked when the button is long clicked.

`listener` - Callback to be invoked when the button is pressed.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TwoStateButton(primaryImageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, primaryContentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, primaryImageTintResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, secondaryImageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = primaryImageResource, secondaryContentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = primaryContentDescription, secondaryImageTintResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = primaryImageTintResource, isInPrimaryState: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true }, disableInSecondaryState: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, longClickListener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null, listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>A button that either shows an primary state or an secondary state based on the provided isInPrimaryState lambda. |

### Properties

| Name | Summary |
|---|---|
| [disableInSecondaryState](disable-in-secondary-state.md) | `val disableInSecondaryState: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Optional boolean to disable the button when in secondary state. |
| [isInPrimaryState](is-in-primary-state.md) | `val isInPrimaryState: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Lambda to return true/false to indicate if this button should be primary or secondary. |
| [primaryContentDescription](primary-content-description.md) | `val primaryContentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The button's primary content description, used for accessibility support. |
| [primaryImageResource](primary-image-resource.md) | `val primaryImageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>ID of a drawable resource to be shown as primary icon. |
| [primaryImageTintResource](primary-image-tint-resource.md) | `val primaryImageTintResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Optional ID of color resource to tint the primary icon. |
| [secondaryContentDescription](secondary-content-description.md) | `val secondaryContentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Optional secondary content description for button, for accessibility support. |
| [secondaryImageResource](secondary-image-resource.md) | `val secondaryImageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Optional ID of a different drawable resource to be shown as secondary icon. |
| [secondaryImageTintResource](secondary-image-tint-resource.md) | `val secondaryImageTintResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Optional ID of secondary color resource to tint the icon. |

### Inherited Properties

| Name | Summary |
|---|---|
| [contentDescription](../-button/content-description.md) | `val contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The button's content description, used for accessibility support. |
| [iconTintColorResource](../-button/icon-tint-color-resource.md) | `val iconTintColorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Optional ID of color resource to tint the icon. |
| [imageResource](../-button/image-resource.md) | `val imageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>ID of a drawable resource to be shown as icon. |
| [isEnabled](../-button/is-enabled.md) | `val isEnabled: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Lambda to return true/false to indicate if this button should be enabled or disabled. |
| [listener](../-button/listener.md) | `val listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Callback to be invoked when the button is pressed. |
| [longClickListener](../-button/long-click-listener.md) | `val longClickListener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Callback to be invoked when the button is long clicked. |
