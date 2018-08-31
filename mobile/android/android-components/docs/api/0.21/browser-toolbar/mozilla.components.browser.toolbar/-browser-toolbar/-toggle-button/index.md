---
title: BrowserToolbar.ToggleButton - 
---

[mozilla.components.browser.toolbar](../../index.html) / [BrowserToolbar](../index.html) / [ToggleButton](./index.html)

# ToggleButton

`class ToggleButton : ActionToggleButton`

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

### Constructors

| [&lt;init&gt;](-init-.html) | `ToggleButton(imageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, imageResourceSelected: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, contentDescriptionSelected: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true }, selected: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, background: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`? = null, listener: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>An action button with two states, selected and unselected. When the button is pressed, the state changes automatically. |

### Functions

| [createView](create-view.html) | `open fun createView(parent: ViewGroup): View` |

