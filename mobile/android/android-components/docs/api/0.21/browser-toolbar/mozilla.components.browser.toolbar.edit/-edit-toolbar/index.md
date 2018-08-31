---
title: EditToolbar - 
---

[mozilla.components.browser.toolbar.edit](../index.html) / [EditToolbar](./index.html)

# EditToolbar

`class EditToolbar : ViewGroup`

Sub-component of the browser toolbar responsible for allowing the user to edit the URL.

Structure:
+---------------------------------+---------+------+
|                 url             | actions | exit |
+---------------------------------+---------+------+

* url: Editable URL of the currently displayed website
* actions: Optional action icons injected by other components (e.g. barcode scanner)
* exit: Button that switches back to display mode.

### Constructors

| [&lt;init&gt;](-init-.html) | `EditToolbar(context: Context, toolbar: `[`BrowserToolbar`](../../mozilla.components.browser.toolbar/-browser-toolbar/index.html)`)`<br>Sub-component of the browser toolbar responsible for allowing the user to edit the URL. |

### Functions

| [focus](focus.html) | `fun focus(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Focus the URL editing component and show the virtual keyboard if needed. |
| [onLayout](on-layout.html) | `fun onLayout(changed: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, left: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, top: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, right: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, bottom: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onMeasure](on-measure.html) | `fun onMeasure(widthMeasureSpec: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, heightMeasureSpec: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [updateUrl](update-url.html) | `fun updateUrl(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Updates the URL. This should only be called if the toolbar is not in editing mode. Otherwise this might override the URL the user is currently typing. |

