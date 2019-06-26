[android-components](../../index.md) / [mozilla.components.browser.menu.item](../index.md) / [BrowserMenuItemToolbar](./index.md)

# BrowserMenuItemToolbar

`class BrowserMenuItemToolbar : `[`BrowserMenuItem`](../../mozilla.components.browser.menu/-browser-menu-item/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/item/BrowserMenuItemToolbar.kt#L21)

A toolbar of buttons to show inside the browser menu.

### Types

| Name | Summary |
|---|---|
| [Button](-button/index.md) | `class Button`<br>A button to be shown in a toolbar inside the browser menu. |
| [TwoStateButton](-two-state-button/index.md) | `class TwoStateButton : `[`Button`](-button/index.md)<br>A button that either shows an primary state or an secondary state based on the provided isInPrimaryState lambda. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserMenuItemToolbar(items: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Button`](-button/index.md)`>)`<br>A toolbar of buttons to show inside the browser menu. |

### Properties

| Name | Summary |
|---|---|
| [visible](visible.md) | `var visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Lambda expression that returns true if this item should be shown in the menu. Returns false if this item should be hidden. |

### Functions

| Name | Summary |
|---|---|
| [bind](bind.md) | `fun bind(menu: `[`BrowserMenu`](../../mozilla.components.browser.menu/-browser-menu/index.md)`, view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to display the data of this item using the passed view. |
| [getLayoutResource](get-layout-resource.md) | `fun getLayoutResource(): <ERROR CLASS>`<br>Returns the layout resource ID of the layout to be inflated for showing a menu item of this type. |
| [invalidate](invalidate.md) | `fun invalidate(view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to update the displayed data of this item using the passed view. |
