[android-components](../../../index.md) / [mozilla.components.browser.menu.item](../../index.md) / [BrowserMenuItemToolbar](../index.md) / [TwoStateButton](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`TwoStateButton(@DrawableRes primaryImageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, primaryContentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, @ColorRes primaryImageTintResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, @DrawableRes secondaryImageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = primaryImageResource, secondaryContentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = primaryContentDescription, @ColorRes secondaryImageTintResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = primaryImageTintResource, isInPrimaryState: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true }, disableInSecondaryState: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, longClickListener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null, listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`

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