---
title: SimpleBrowserMenuItem - 
---

[mozilla.components.browser.menu.item](../index.html) / [SimpleBrowserMenuItem](./index.html)

# SimpleBrowserMenuItem

`class SimpleBrowserMenuItem : `[`BrowserMenuItem`](../../mozilla.components.browser.menu/-browser-menu-item/index.html)

A simple browser menu item displaying text.

### Parameters

`label` - The visible label of this menu item.

`listener` - Callback to be invoked when this menu item is clicked.

### Constructors

| [&lt;init&gt;](-init-.html) | `SimpleBrowserMenuItem(label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>A simple browser menu item displaying text. |

### Properties

| [visible](visible.html) | `var visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Lambda expression that returns true if this item should be shown in the menu. Returns false if this item should be hidden. |

### Functions

| [bind](bind.html) | `fun bind(menu: `[`BrowserMenu`](../../mozilla.components.browser.menu/-browser-menu/index.html)`, view: View): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to display the data of this item using the passed view. |
| [getLayoutResource](get-layout-resource.html) | `fun getLayoutResource(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Returns the layout resource ID of the layout to be inflated for showing a menu item of this type. |

