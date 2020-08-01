[android-components](../../../index.md) / [mozilla.components.concept.toolbar](../../index.md) / [Toolbar](../index.md) / [ActionButton](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`ActionButton(imageDrawable: <ERROR CLASS>? = null, contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true }, background: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0, padding: `[`Padding`](../../../mozilla.components.support.base.android/-padding/index.md)`? = null, listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`

An action button to be added to the toolbar.

### Parameters

`imageDrawable` - The drawable to be shown.

`contentDescription` - The content description to use.

`visible` - Lambda that returns true or false to indicate whether this button should be shown.

`padding` - A optional custom padding.

`listener` - Callback that will be invoked whenever the button is pressed