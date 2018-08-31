---
title: Toolbar.ActionToggleButton - 
---

[mozilla.components.concept.toolbar](../../index.html) / [Toolbar](../index.html) / [ActionToggleButton](./index.html)

# ActionToggleButton

`open class ActionToggleButton : `[`Action`](../-action/index.html)

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

| [&lt;init&gt;](-init-.html) | `ActionToggleButton(imageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, imageResourceSelected: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, contentDescriptionSelected: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true }, selected: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, background: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`? = null, listener: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>An action button with two states, selected and unselected. When the button is pressed, the state changes automatically. |

### Properties

| [visible](visible.html) | `open val visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Lambda that returns true or false to indicate whether this button should be shown. |

### Functions

| [bind](bind.html) | `open fun bind(view: View): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [createView](create-view.html) | `open fun createView(parent: ViewGroup): View` |
| [isSelected](is-selected.html) | `fun isSelected(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns the current selected state of the action. |
| [setSelected](set-selected.html) | `fun setSelected(selected: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, notifyListener: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Changes the selected state of the action. |
| [toggle](toggle.html) | `fun toggle(notifyListener: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Changes the selected state of the action to the inverse of its current state. |

