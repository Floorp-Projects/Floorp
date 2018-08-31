---
title: BrowserMenuItem - 
---

[mozilla.components.browser.menu](../index.html) / [BrowserMenuItem](./index.html)

# BrowserMenuItem

`interface BrowserMenuItem`

Interface to be implemented by menu items to be shown in the browser menu.

### Properties

| [visible](visible.html) | `abstract val visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Lambda expression that returns true if this item should be shown in the menu. Returns false if this item should be hidden. |

### Functions

| [bind](bind.html) | `abstract fun bind(menu: `[`BrowserMenu`](../-browser-menu/index.html)`, view: View): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to display the data of this item using the passed view. |
| [getLayoutResource](get-layout-resource.html) | `abstract fun getLayoutResource(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Returns the layout resource ID of the layout to be inflated for showing a menu item of this type. |

### Inheritors

| [BrowserMenuItemToolbar](../../mozilla.components.browser.menu.item/-browser-menu-item-toolbar/index.html) | `class BrowserMenuItemToolbar : `[`BrowserMenuItem`](./index.md)<br>A toolbar of buttons to show inside the browser menu. |
| [SimpleBrowserMenuCheckbox](../../mozilla.components.browser.menu.item/-simple-browser-menu-checkbox/index.html) | `class SimpleBrowserMenuCheckbox : `[`BrowserMenuItem`](./index.md)<br>A simple browser menu checkbox. |
| [SimpleBrowserMenuItem](../../mozilla.components.browser.menu.item/-simple-browser-menu-item/index.html) | `class SimpleBrowserMenuItem : `[`BrowserMenuItem`](./index.md)<br>A simple browser menu item displaying text. |

