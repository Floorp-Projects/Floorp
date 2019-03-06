[android-components](../../index.md) / [mozilla.components.browser.menu.item](../index.md) / [BrowserMenuImageText](./index.md)

# BrowserMenuImageText

`class BrowserMenuImageText : `[`BrowserMenuItem`](../../mozilla.components.browser.menu/-browser-menu-item/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/item/BrowserMenuImageText.kt#L28)

A menu item for displaying text with an image icon.

### Parameters

`label` - The visible label of this menu item.

`imageResource` - ID of a drawable resource to be shown as icon.

`iconTintColorResource` - Optional ID of color resource to tint the icon.

`textColorResource` - Optional ID of color resource to tint the text.

`listener` - Callback to be invoked when this menu item is clicked.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserMenuImageText(label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, imageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, iconTintColorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, textColorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {})`<br>A menu item for displaying text with an image icon. |

### Properties

| Name | Summary |
|---|---|
| [visible](visible.md) | `var visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Lambda expression that returns true if this item should be shown in the menu. Returns false if this item should be hidden. |

### Functions

| Name | Summary |
|---|---|
| [bind](bind.md) | `fun bind(menu: `[`BrowserMenu`](../../mozilla.components.browser.menu/-browser-menu/index.md)`, view: `[`View`](https://developer.android.com/reference/android/view/View.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to display the data of this item using the passed view. |
| [getLayoutResource](get-layout-resource.md) | `fun getLayoutResource(): <ERROR CLASS>`<br>Returns the layout resource ID of the layout to be inflated for showing a menu item of this type. |
