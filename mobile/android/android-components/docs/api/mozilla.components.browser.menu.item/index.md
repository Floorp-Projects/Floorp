[android-components](../index.md) / [mozilla.components.browser.menu.item](./index.md)

## Package mozilla.components.browser.menu.item

### Types

| Name | Summary |
|---|---|
| [AbstractParentBrowserMenuItem](-abstract-parent-browser-menu-item/index.md) | `abstract class AbstractParentBrowserMenuItem : `[`BrowserMenuItem`](../mozilla.components.browser.menu/-browser-menu-item/index.md)<br>An abstract menu item for handling nested sub menu items on view click. |
| [BackPressMenuItem](-back-press-menu-item/index.md) | `class BackPressMenuItem : `[`BrowserMenuImageText`](-browser-menu-image-text/index.md)<br>A back press menu item for a nested sub menu entry. |
| [BrowserMenuCategory](-browser-menu-category/index.md) | `class BrowserMenuCategory : `[`BrowserMenuItem`](../mozilla.components.browser.menu/-browser-menu-item/index.md)<br>A browser menu item displaying styleable text, usable for menu categories |
| [BrowserMenuCheckbox](-browser-menu-checkbox/index.md) | `class BrowserMenuCheckbox : `[`BrowserMenuCompoundButton`](-browser-menu-compound-button/index.md)<br>A simple browser menu checkbox. |
| [BrowserMenuCompoundButton](-browser-menu-compound-button/index.md) | `abstract class BrowserMenuCompoundButton : `[`BrowserMenuItem`](../mozilla.components.browser.menu/-browser-menu-item/index.md)<br>A browser menu compound button. A basic sub-class would only have to provide a layout resource to satisfy [BrowserMenuItem.getLayoutResource](../mozilla.components.browser.menu/-browser-menu-item/get-layout-resource.md) which contains a [View](#) that inherits from [CompoundButton](#). |
| [BrowserMenuDivider](-browser-menu-divider/index.md) | `class BrowserMenuDivider : `[`BrowserMenuItem`](../mozilla.components.browser.menu/-browser-menu-item/index.md)<br>A browser menu item to display a horizontal divider. |
| [BrowserMenuHighlightableItem](-browser-menu-highlightable-item/index.md) | `class BrowserMenuHighlightableItem : `[`BrowserMenuImageText`](-browser-menu-image-text/index.md)`, `[`HighlightableMenuItem`](../mozilla.components.browser.menu/-highlightable-menu-item/index.md)<br>A menu item for displaying text with an image icon and a highlight state which sets the background of the menu item and a second image icon to the right of the text. |
| [BrowserMenuHighlightableSwitch](-browser-menu-highlightable-switch/index.md) | `class BrowserMenuHighlightableSwitch : `[`BrowserMenuCompoundButton`](-browser-menu-compound-button/index.md)`, `[`HighlightableMenuItem`](../mozilla.components.browser.menu/-highlightable-menu-item/index.md)<br>A browser menu switch that can show a highlighted icon. |
| [BrowserMenuImageSwitch](-browser-menu-image-switch/index.md) | `class BrowserMenuImageSwitch : `[`BrowserMenuCompoundButton`](-browser-menu-compound-button/index.md)<br>A simple browser menu switch. |
| [BrowserMenuImageText](-browser-menu-image-text/index.md) | `open class BrowserMenuImageText : `[`BrowserMenuItem`](../mozilla.components.browser.menu/-browser-menu-item/index.md)<br>A menu item for displaying text with an image icon. |
| [BrowserMenuItemToolbar](-browser-menu-item-toolbar/index.md) | `class BrowserMenuItemToolbar : `[`BrowserMenuItem`](../mozilla.components.browser.menu/-browser-menu-item/index.md)<br>A toolbar of buttons to show inside the browser menu. |
| [BrowserMenuSwitch](-browser-menu-switch/index.md) | `class BrowserMenuSwitch : `[`BrowserMenuCompoundButton`](-browser-menu-compound-button/index.md)<br>A simple browser menu switch. |
| [ParentBrowserMenuItem](-parent-browser-menu-item/index.md) | `class ParentBrowserMenuItem : `[`AbstractParentBrowserMenuItem`](-abstract-parent-browser-menu-item/index.md)<br>A parent menu item for displaying text and an image icon with a nested sub menu. It handles back pressing if the sub menu contains a [BackPressMenuItem](-back-press-menu-item/index.md). |
| [SimpleBrowserMenuHighlightableItem](-simple-browser-menu-highlightable-item/index.md) | `class SimpleBrowserMenuHighlightableItem : `[`BrowserMenuItem`](../mozilla.components.browser.menu/-browser-menu-item/index.md)<br>A menu item for displaying text with a highlight state which sets the background of the menu item. |
| [SimpleBrowserMenuItem](-simple-browser-menu-item/index.md) | `class SimpleBrowserMenuItem : `[`BrowserMenuItem`](../mozilla.components.browser.menu/-browser-menu-item/index.md)<br>A simple browser menu item displaying text. |
| [WebExtensionBrowserMenuItem](-web-extension-browser-menu-item/index.md) | `class WebExtensionBrowserMenuItem : `[`BrowserMenuItem`](../mozilla.components.browser.menu/-browser-menu-item/index.md)<br>A browser menu item displaying a web extension action. |

### Functions

| Name | Summary |
|---|---|
| [setBadgeText](set-badge-text.md) | `fun <ERROR CLASS>.setBadgeText(badgeText: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the badgeText and the visibility of the TextView based on empty/nullability of the badgeText. |
