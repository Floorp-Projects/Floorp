[android-components](../../../index.md) / [mozilla.components.browser.menu.item](../../index.md) / [BrowserMenuItemToolbar](../index.md) / [Button](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`Button(@DrawableRes imageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, @ColorRes iconTintColorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, isEnabled: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true }, longClickListener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null, listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`

A button to be shown in a toolbar inside the browser menu.

### Parameters

`imageResource` - ID of a drawable resource to be shown as icon.

`contentDescription` - The button's content description, used for accessibility support.

`iconTintColorResource` - Optional ID of color resource to tint the icon.

`isEnabled` - Lambda to return true/false to indicate if this button should be enabled or disabled.

`longClickListener` - Callback to be invoked when the button is long clicked.

`listener` - Callback to be invoked when the button is pressed.