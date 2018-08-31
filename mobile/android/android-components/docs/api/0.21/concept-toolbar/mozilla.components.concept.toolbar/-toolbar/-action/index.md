---
title: Toolbar.Action - 
---

[mozilla.components.concept.toolbar](../../index.html) / [Toolbar](../index.html) / [Action](./index.html)

# Action

`interface Action`

Generic interface for actions to be added to the toolbar.

### Properties

| [visible](visible.html) | `open val visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Functions

| [bind](bind.html) | `abstract fun bind(view: View): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [createView](create-view.html) | `abstract fun createView(parent: ViewGroup): View` |

### Inheritors

| [ActionButton](../-action-button/index.html) | `open class ActionButton : `[`Action`](./index.md)<br>An action button to be added to the toolbar. |
| [ActionImage](../-action-image/index.html) | `open class ActionImage : `[`Action`](./index.md)<br>An action that just shows a static, non-clickable image. |
| [ActionSpace](../-action-space/index.html) | `open class ActionSpace : `[`Action`](./index.md)<br>An "empty" action with a desired width to be used as "placeholder". |
| [ActionToggleButton](../-action-toggle-button/index.html) | `open class ActionToggleButton : `[`Action`](./index.md)<br>An action button with two states, selected and unselected. When the button is pressed, the state changes automatically. |

