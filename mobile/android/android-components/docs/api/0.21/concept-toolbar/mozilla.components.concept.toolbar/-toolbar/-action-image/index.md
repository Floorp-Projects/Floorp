---
title: Toolbar.ActionImage - 
---

[mozilla.components.concept.toolbar](../../index.html) / [Toolbar](../index.html) / [ActionImage](./index.html)

# ActionImage

`open class ActionImage : `[`Action`](../-action/index.html)

An action that just shows a static, non-clickable image.

### Parameters

`imageResource` - The drawable to be shown.

`contentDescription` - Optional content description to be used. If no content description
    is provided then this view will be treated as not important for
    accessibility.

### Constructors

| [&lt;init&gt;](-init-.html) | `ActionImage(imageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null)`<br>An action that just shows a static, non-clickable image. |

### Inherited Properties

| [visible](../-action/visible.html) | `open val visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Functions

| [bind](bind.html) | `open fun bind(view: View): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [createView](create-view.html) | `open fun createView(parent: ViewGroup): View` |

