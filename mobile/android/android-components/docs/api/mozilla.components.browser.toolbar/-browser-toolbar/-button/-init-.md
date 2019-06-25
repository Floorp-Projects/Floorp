[android-components](../../../index.md) / [mozilla.components.browser.toolbar](../../index.md) / [BrowserToolbar](../index.md) / [Button](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`Button(imageDrawable: <ERROR CLASS>, contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true }, @DrawableRes background: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0, padding: `[`Padding`](../../../mozilla.components.support.base.android/-padding/index.md)` = DEFAULT_PADDING, listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`

An action button to be added to the toolbar.

### Parameters

`imageDrawable` - The drawable to be shown.

`contentDescription` - The content description to use.

`visible` - Lambda that returns true or false to indicate whether this button should be shown.

`background` - A custom (stateful) background drawable resource to be used.

`padding` - a custom [Padding](../../../mozilla.components.support.base.android/-padding/index.md) for this Button.

`listener` - Callback that will be invoked whenever the button is pressed