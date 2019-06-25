[android-components](../../index.md) / [mozilla.components.browser.menu](../index.md) / [BrowserMenuItem](./index.md)

# BrowserMenuItem

`interface BrowserMenuItem` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/BrowserMenuItem.kt#L12)

Interface to be implemented by menu items to be shown in the browser menu.

### Properties

| Name | Summary |
|---|---|
| [visible](visible.md) | `abstract val visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Lambda expression that returns true if this item should be shown in the menu. Returns false if this item should be hidden. |

### Functions

| Name | Summary |
|---|---|
| [bind](bind.md) | `abstract fun bind(menu: `[`BrowserMenu`](../-browser-menu/index.md)`, view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to display the data of this item using the passed view. |
| [getLayoutResource](get-layout-resource.md) | `abstract fun getLayoutResource(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Returns the layout resource ID of the layout to be inflated for showing a menu item of this type. |
| [invalidate](invalidate.md) | `open fun invalidate(view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to update the displayed data of this item using the passed view. |

### Inheritors

| Name | Summary |
|---|---|
| [BrowserMenuCompoundButton](../../mozilla.components.browser.menu.item/-browser-menu-compound-button/index.md) | `abstract class BrowserMenuCompoundButton : `[`BrowserMenuItem`](./index.md)<br>A browser menu compound button. A basic sub-class would only have to provide a layout resource to satisfy [BrowserMenuItem.getLayoutResource](get-layout-resource.md) which contains a [View](#) that inherits from [CompoundButton](#). |
| [BrowserMenuDivider](../../mozilla.components.browser.menu.item/-browser-menu-divider/index.md) | `class BrowserMenuDivider : `[`BrowserMenuItem`](./index.md)<br>A browser menu item to display a horizontal divider. |
| [BrowserMenuImageText](../../mozilla.components.browser.menu.item/-browser-menu-image-text/index.md) | `open class BrowserMenuImageText : `[`BrowserMenuItem`](./index.md)<br>A menu item for displaying text with an image icon. |
| [BrowserMenuItemToolbar](../../mozilla.components.browser.menu.item/-browser-menu-item-toolbar/index.md) | `class BrowserMenuItemToolbar : `[`BrowserMenuItem`](./index.md)<br>A toolbar of buttons to show inside the browser menu. |
| [SimpleBrowserMenuItem](../../mozilla.components.browser.menu.item/-simple-browser-menu-item/index.md) | `class SimpleBrowserMenuItem : `[`BrowserMenuItem`](./index.md)<br>A simple browser menu item displaying text. |
