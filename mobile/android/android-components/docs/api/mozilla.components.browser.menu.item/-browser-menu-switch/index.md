[android-components](../../index.md) / [mozilla.components.browser.menu.item](../index.md) / [BrowserMenuSwitch](./index.md)

# BrowserMenuSwitch

`class BrowserMenuSwitch : `[`BrowserMenuCompoundButton`](../-browser-menu-compound-button/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/item/BrowserMenuSwitch.kt#L16)

A simple browser menu switch.

### Parameters

`label` - The visible label of this menu item.

`initialState` - The initial value the checkbox should have.

`listener` - Callback to be invoked when this menu item is checked.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserMenuSwitch(label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, initialState: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { false }, listener: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>A simple browser menu switch. |

### Inherited Properties

| Name | Summary |
|---|---|
| [label](../-browser-menu-compound-button/label.md) | `val label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The visible label of this menu item. |
| [visible](../-browser-menu-compound-button/visible.md) | `open var visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Lambda expression that returns true if this item should be shown in the menu. Returns false if this item should be hidden. |

### Functions

| Name | Summary |
|---|---|
| [getLayoutResource](get-layout-resource.md) | `fun getLayoutResource(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Returns the layout resource ID of the layout to be inflated for showing a menu item of this type. |

### Inherited Functions

| Name | Summary |
|---|---|
| [bind](../-browser-menu-compound-button/bind.md) | `open fun bind(menu: `[`BrowserMenu`](../../mozilla.components.browser.menu/-browser-menu/index.md)`, view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to display the data of this item using the passed view. |
