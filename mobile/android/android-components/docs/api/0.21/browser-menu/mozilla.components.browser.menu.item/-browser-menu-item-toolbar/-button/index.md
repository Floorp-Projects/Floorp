---
title: BrowserMenuItemToolbar.Button - 
---

[mozilla.components.browser.menu.item](../../index.html) / [BrowserMenuItemToolbar](../index.html) / [Button](./index.html)

# Button

`class Button`

A button to be shown in a toolbar inside the browser menu.

### Parameters

`imageResource` - ID of a drawable resource to be shown as icon.

`contentDescription` - The button's content description, used for accessibility support.

`iconTintColorResource` - Optional ID of color resource to tint the icon.

`listener` - Callback to be invoked when the button is pressed.

### Constructors

| [&lt;init&gt;](-init-.html) | `Button(imageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, iconTintColorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0, listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>A button to be shown in a toolbar inside the browser menu. |

### Properties

| [contentDescription](content-description.html) | `val contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The button's content description, used for accessibility support. |
| [iconTintColorResource](icon-tint-color-resource.html) | `val iconTintColorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Optional ID of color resource to tint the icon. |
| [imageResource](image-resource.html) | `val imageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>ID of a drawable resource to be shown as icon. |
| [listener](listener.html) | `val listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Callback to be invoked when the button is pressed. |

