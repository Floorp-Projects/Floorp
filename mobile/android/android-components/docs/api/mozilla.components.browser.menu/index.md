[android-components](../index.md) / [mozilla.components.browser.menu](./index.md)

## Package mozilla.components.browser.menu

### Types

| Name | Summary |
|---|---|
| [BrowserMenu](-browser-menu/index.md) | `open class BrowserMenu`<br>A popup menu composed of BrowserMenuItem objects. |
| [BrowserMenuBuilder](-browser-menu-builder/index.md) | `open class BrowserMenuBuilder`<br>Helper class for building browser menus. |
| [BrowserMenuHighlight](-browser-menu-highlight/index.md) | `sealed class BrowserMenuHighlight`<br>Describes how to display a [mozilla.components.browser.menu.item.BrowserMenuHighlightableItem](../mozilla.components.browser.menu.item/-browser-menu-highlightable-item/index.md) when it is highlighted. |
| [BrowserMenuItem](-browser-menu-item/index.md) | `interface BrowserMenuItem`<br>Interface to be implemented by menu items to be shown in the browser menu. |
| [BrowserMenuItemViewHolder](-browser-menu-item-view-holder/index.md) | `class BrowserMenuItemViewHolder : ViewHolder` |
| [HighlightableMenuItem](-highlightable-menu-item/index.md) | `interface HighlightableMenuItem`<br>Indicates that a menu item shows a highlight. |
| [WebExtensionBrowserMenu](-web-extension-browser-menu/index.md) | `class WebExtensionBrowserMenu : `[`BrowserMenu`](-browser-menu/index.md)<br>A [BrowserMenu](-browser-menu/index.md) capable of displaying browser and page actions from web extensions. |
| [WebExtensionBrowserMenuBuilder](-web-extension-browser-menu-builder/index.md) | `class WebExtensionBrowserMenuBuilder : `[`BrowserMenuBuilder`](-browser-menu-builder/index.md)<br>Browser menu builder with web extension support. It allows [WebExtensionBrowserMenu](-web-extension-browser-menu/index.md) to add web extension browser actions in a nested menu item. If there are no web extensions installed, the web extension menu item would return an add-on manager menu item instead. |
