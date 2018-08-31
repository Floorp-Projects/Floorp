---
title: BrowserMenuItemToolbar - 
---

[mozilla.components.browser.menu.item](../index.html) / [BrowserMenuItemToolbar](./index.html)

# BrowserMenuItemToolbar

`class BrowserMenuItemToolbar : `[`BrowserMenuItem`](../../mozilla.components.browser.menu/-browser-menu-item/index.html)

A toolbar of buttons to show inside the browser menu.

### Types

| [Button](-button/index.html) | `class Button`<br>A button to be shown in a toolbar inside the browser menu. |

### Constructors

| [&lt;init&gt;](-init-.html) | `BrowserMenuItemToolbar(items: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Button`](-button/index.html)`>)`<br>A toolbar of buttons to show inside the browser menu. |

### Properties

| [visible](visible.html) | `val visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Lambda expression that returns true if this item should be shown in the menu. Returns false if this item should be hidden. |

### Functions

| [bind](bind.html) | `fun bind(menu: `[`BrowserMenu`](../../mozilla.components.browser.menu/-browser-menu/index.html)`, view: View): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to display the data of this item using the passed view. |
| [getLayoutResource](get-layout-resource.html) | `fun getLayoutResource(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Returns the layout resource ID of the layout to be inflated for showing a menu item of this type. |

