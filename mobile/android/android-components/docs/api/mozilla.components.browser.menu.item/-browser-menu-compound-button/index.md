[android-components](../../index.md) / [mozilla.components.browser.menu.item](../index.md) / [BrowserMenuCompoundButton](./index.md)

# BrowserMenuCompoundButton

`abstract class BrowserMenuCompoundButton : `[`BrowserMenuItem`](../../mozilla.components.browser.menu/-browser-menu-item/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/item/BrowserMenuCompoundButton.kt#L22)

A browser menu compound button. A basic sub-class would only have to provide a layout resource to
satisfy [BrowserMenuItem.getLayoutResource](../../mozilla.components.browser.menu/-browser-menu-item/get-layout-resource.md) which contains a [View](#) that inherits from [CompoundButton](#).

### Parameters

`label` - The visible label of this menu item.

`initialState` - The initial value the checkbox should have.

`listener` - Callback to be invoked when this menu item is checked.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserMenuCompoundButton(label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, initialState: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { false }, listener: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>A browser menu compound button. A basic sub-class would only have to provide a layout resource to satisfy [BrowserMenuItem.getLayoutResource](../../mozilla.components.browser.menu/-browser-menu-item/get-layout-resource.md) which contains a [View](#) that inherits from [CompoundButton](#). |

### Properties

| Name | Summary |
|---|---|
| [label](label.md) | `val label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The visible label of this menu item. |
| [visible](visible.md) | `open var visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Lambda expression that returns true if this item should be shown in the menu. Returns false if this item should be hidden. |

### Functions

| Name | Summary |
|---|---|
| [bind](bind.md) | `open fun bind(menu: `[`BrowserMenu`](../../mozilla.components.browser.menu/-browser-menu/index.md)`, view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to display the data of this item using the passed view. |

### Inherited Functions

| Name | Summary |
|---|---|
| [getLayoutResource](../../mozilla.components.browser.menu/-browser-menu-item/get-layout-resource.md) | `abstract fun getLayoutResource(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Returns the layout resource ID of the layout to be inflated for showing a menu item of this type. |
| [invalidate](../../mozilla.components.browser.menu/-browser-menu-item/invalidate.md) | `open fun invalidate(view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to update the displayed data of this item using the passed view. |

### Inheritors

| Name | Summary |
|---|---|
| [BrowserMenuCheckbox](../-browser-menu-checkbox/index.md) | `class BrowserMenuCheckbox : `[`BrowserMenuCompoundButton`](./index.md)<br>A simple browser menu checkbox. |
| [BrowserMenuImageSwitch](../-browser-menu-image-switch/index.md) | `class BrowserMenuImageSwitch : `[`BrowserMenuCompoundButton`](./index.md)<br>A simple browser menu switch. |
| [BrowserMenuSwitch](../-browser-menu-switch/index.md) | `class BrowserMenuSwitch : `[`BrowserMenuCompoundButton`](./index.md)<br>A simple browser menu switch. |
