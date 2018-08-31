---
title: Toolbar.ActionButton - 
---

[mozilla.components.concept.toolbar](../../index.html) / [Toolbar](../index.html) / [ActionButton](./index.html)

# ActionButton

`open class ActionButton : `[`Action`](../-action/index.html)

An action button to be added to the toolbar.

### Parameters

`imageResource` - The drawable to be shown.

`contentDescription` - The content description to use.

`visible` - Lambda that returns true or false to indicate whether this button should be shown.

`background` - A custom (stateful) background drawable resource to be used.

`listener` - Callback that will be invoked whenever the button is pressed

### Constructors

| [&lt;init&gt;](-init-.html) | `ActionButton(imageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true }, background: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`? = null, listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>An action button to be added to the toolbar. |

### Properties

| [visible](visible.html) | `open val visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Lambda that returns true or false to indicate whether this button should be shown. |

### Functions

| [bind](bind.html) | `open fun bind(view: View): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [createView](create-view.html) | `open fun createView(parent: ViewGroup): View` |

