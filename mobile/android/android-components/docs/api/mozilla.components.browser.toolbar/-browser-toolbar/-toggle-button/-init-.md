[android-components](../../../index.md) / [mozilla.components.browser.toolbar](../../index.md) / [BrowserToolbar](../index.md) / [ToggleButton](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`ToggleButton(image: <ERROR CLASS>, imageSelected: <ERROR CLASS>, contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, contentDescriptionSelected: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true }, selected: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, @DrawableRes background: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0, padding: `[`Padding`](../../../mozilla.components.support.base.android/-padding/index.md)` = DEFAULT_PADDING, listener: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`

An action button with two states, selected and unselected. When the button is pressed, the
state changes automatically.

### Parameters

`image` - The drawable to be shown if the button is in unselected state.

`imageSelected` - The drawable to be shown if the button is in selected state.

`contentDescription` - The content description to use if the button is in unselected state.

`contentDescriptionSelected` - The content description to use if the button is in selected state.

`visible` - Lambda that returns true or false to indicate whether this button should be shown.

`selected` - Sets whether this button should be selected initially.

`background` - A custom (stateful) background drawable resource to be used.

`padding` - a custom [Padding](../../../mozilla.components.support.base.android/-padding/index.md) for this Button.

`listener` - Callback that will be invoked whenever the checked state changes.