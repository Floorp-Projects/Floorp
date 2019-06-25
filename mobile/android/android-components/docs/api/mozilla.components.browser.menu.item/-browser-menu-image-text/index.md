[android-components](../../index.md) / [mozilla.components.browser.menu.item](../index.md) / [BrowserMenuImageText](./index.md)

# BrowserMenuImageText

`open class BrowserMenuImageText : `[`BrowserMenuItem`](../../mozilla.components.browser.menu/-browser-menu-item/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/item/BrowserMenuImageText.kt#L41)

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
| [visible](visible.md) | `open var visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Lambda expression that returns true if this item should be shown in the menu. Returns false if this item should be hidden. |

### Functions

| Name | Summary |
|---|---|
| [bind](bind.md) | `open fun bind(menu: `[`BrowserMenu`](../../mozilla.components.browser.menu/-browser-menu/index.md)`, view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to display the data of this item using the passed view. |
| [getLayoutResource](get-layout-resource.md) | `open fun getLayoutResource(): <ERROR CLASS>`<br>Returns the layout resource ID of the layout to be inflated for showing a menu item of this type. |

### Inherited Functions

| Name | Summary |
|---|---|
| [invalidate](../../mozilla.components.browser.menu/-browser-menu-item/invalidate.md) | `open fun invalidate(view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to update the displayed data of this item using the passed view. |

### Inheritors

| Name | Summary |
|---|---|
| [BrowserMenuHighlightableItem](../-browser-menu-highlightable-item/index.md) | `class BrowserMenuHighlightableItem : `[`BrowserMenuImageText`](./index.md)<br>A menu item for displaying text with an image icon and a highlight state which sets the background of the menu item and a second image icon to the right of the text. |
