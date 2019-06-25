[android-components](../../../index.md) / [mozilla.components.concept.toolbar](../../index.md) / [Toolbar](../index.md) / [ActionToggleButton](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`ActionToggleButton(imageDrawable: <ERROR CLASS>, imageSelectedDrawable: <ERROR CLASS>, contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, contentDescriptionSelected: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true }, selected: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, @DrawableRes background: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0, padding: `[`Padding`](../../../mozilla.components.support.base.android/-padding/index.md)`? = null, listener: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`

An action button with two states, selected and unselected. When the button is pressed, the
state changes automatically.

### Parameters

`imageDrawable` - The drawable to be shown if the button is in unselected state.

`imageSelectedDrawable` - The  drawable to be shown if the button is in selected state.

`contentDescription` - The content description to use if the button is in unselected state.

`contentDescriptionSelected` - The content description to use if the button is in selected state.

`visible` - Lambda that returns true or false to indicate whether this button should be shown.

`selected` - Sets whether this button should be selected initially.

`padding` - A optional custom padding.

`listener` - Callback that will be invoked whenever the checked state changes.