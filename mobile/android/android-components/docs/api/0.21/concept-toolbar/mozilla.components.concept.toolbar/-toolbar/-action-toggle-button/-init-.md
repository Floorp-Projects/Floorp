---
title: Toolbar.ActionToggleButton.<init> - 
---

[mozilla.components.concept.toolbar](../../index.html) / [Toolbar](../index.html) / [ActionToggleButton](index.html) / [&lt;init&gt;](./-init-.html)

# &lt;init&gt;

`ActionToggleButton(@DrawableRes imageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, @DrawableRes imageResourceSelected: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, contentDescriptionSelected: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true }, selected: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, @DrawableRes background: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`? = null, listener: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`

An action button with two states, selected and unselected. When the button is pressed, the
state changes automatically.

### Parameters

`imageResource` - The drawable to be shown if the button is in unselected state.

`imageResourceSelected` - The drawable to be shown if the button is in selected state.

`contentDescription` - The content description to use if the button is in unselected state.

`contentDescriptionSelected` - The content description to use if the button is in selected state.

`visible` - Lambda that returns true or false to indicate whether this button should be shown.

`selected` - Sets whether this button should be selected initially.

`background` - A custom (stateful) background drawable resource to be used.

`listener` - Callback that will be invoked whenever the checked state changes.