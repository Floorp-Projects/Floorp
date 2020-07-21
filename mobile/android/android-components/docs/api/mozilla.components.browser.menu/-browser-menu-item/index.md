[android-components](../../index.md) / [mozilla.components.browser.menu](../index.md) / [BrowserMenuItem](./index.md)

# BrowserMenuItem

`interface BrowserMenuItem` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/BrowserMenuItem.kt#L14)

Interface to be implemented by menu items to be shown in the browser menu.

### Properties

| Name | Summary |
|---|---|
| [interactiveCount](interactive-count.md) | `open val interactiveCount: () -> `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Lambda expression that returns the number of interactive elements in this menu item. For example, a simple item will have 1, divider will have 0, and a composite item, like a tool bar, will have several. |
| [visible](visible.md) | `abstract val visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Lambda expression that returns true if this item should be shown in the menu. Returns false if this item should be hidden. |

### Functions

| Name | Summary |
|---|---|
| [asCandidate](as-candidate.md) | `open fun asCandidate(context: <ERROR CLASS>): `[`MenuCandidate`](../../mozilla.components.concept.menu.candidate/-menu-candidate/index.md)`?`<br>Converts the menu item into a menu candidate. |
| [bind](bind.md) | `abstract fun bind(menu: `[`BrowserMenu`](../-browser-menu/index.md)`, view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to display the data of this item using the passed view. |
| [getLayoutResource](get-layout-resource.md) | `abstract fun getLayoutResource(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Returns the layout resource ID of the layout to be inflated for showing a menu item of this type. |
| [invalidate](invalidate.md) | `open fun invalidate(view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to update the displayed data of this item using the passed view. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [AbstractParentBrowserMenuItem](../../mozilla.components.browser.menu.item/-abstract-parent-browser-menu-item/index.md) | `abstract class AbstractParentBrowserMenuItem : `[`BrowserMenuItem`](./index.md)<br>An abstract menu item for handling nested sub menu items on view click. |
| [BrowserMenuCategory](../../mozilla.components.browser.menu.item/-browser-menu-category/index.md) | `class BrowserMenuCategory : `[`BrowserMenuItem`](./index.md)<br>A browser menu item displaying styleable text, usable for menu categories |
| [BrowserMenuCompoundButton](../../mozilla.components.browser.menu.item/-browser-menu-compound-button/index.md) | `abstract class BrowserMenuCompoundButton : `[`BrowserMenuItem`](./index.md)<br>A browser menu compound button. A basic sub-class would only have to provide a layout resource to satisfy [BrowserMenuItem.getLayoutResource](get-layout-resource.md) which contains a [View](#) that inherits from [CompoundButton](#). |
| [BrowserMenuDivider](../../mozilla.components.browser.menu.item/-browser-menu-divider/index.md) | `class BrowserMenuDivider : `[`BrowserMenuItem`](./index.md)<br>A browser menu item to display a horizontal divider. |
| [BrowserMenuImageText](../../mozilla.components.browser.menu.item/-browser-menu-image-text/index.md) | `open class BrowserMenuImageText : `[`BrowserMenuItem`](./index.md)<br>A menu item for displaying text with an image icon. |
| [BrowserMenuItemToolbar](../../mozilla.components.browser.menu.item/-browser-menu-item-toolbar/index.md) | `class BrowserMenuItemToolbar : `[`BrowserMenuItem`](./index.md)<br>A toolbar of buttons to show inside the browser menu. |
| [SimpleBrowserMenuHighlightableItem](../../mozilla.components.browser.menu.item/-simple-browser-menu-highlightable-item/index.md) | `class SimpleBrowserMenuHighlightableItem : `[`BrowserMenuItem`](./index.md)<br>A menu item for displaying text with a highlight state which sets the background of the menu item. |
| [SimpleBrowserMenuItem](../../mozilla.components.browser.menu.item/-simple-browser-menu-item/index.md) | `class SimpleBrowserMenuItem : `[`BrowserMenuItem`](./index.md)<br>A simple browser menu item displaying text. |
| [WebExtensionBrowserMenuItem](../../mozilla.components.browser.menu.item/-web-extension-browser-menu-item/index.md) | `class WebExtensionBrowserMenuItem : `[`BrowserMenuItem`](./index.md)<br>A browser menu item displaying a web extension action. |
