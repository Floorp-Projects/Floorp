[android-components](../../index.md) / [mozilla.components.browser.menu](../index.md) / [HighlightableMenuItem](./index.md)

# HighlightableMenuItem

`interface HighlightableMenuItem` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/BrowserMenuHighlight.kt#L99)

Indicates that a menu item shows a highlight.

### Properties

| Name | Summary |
|---|---|
| [highlight](highlight.md) | `abstract val highlight: `[`BrowserMenuHighlight`](../-browser-menu-highlight/index.md)<br>Highlight object representing how the menu item will be displayed when highlighted. |
| [isHighlighted](is-highlighted.md) | `abstract val isHighlighted: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Whether or not to display the highlight |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [BrowserMenuHighlightableItem](../../mozilla.components.browser.menu.item/-browser-menu-highlightable-item/index.md) | `class BrowserMenuHighlightableItem : `[`BrowserMenuImageText`](../../mozilla.components.browser.menu.item/-browser-menu-image-text/index.md)`, `[`HighlightableMenuItem`](./index.md)<br>A menu item for displaying text with an image icon and a highlight state which sets the background of the menu item and a second image icon to the right of the text. |
| [BrowserMenuHighlightableSwitch](../../mozilla.components.browser.menu.item/-browser-menu-highlightable-switch/index.md) | `class BrowserMenuHighlightableSwitch : `[`BrowserMenuCompoundButton`](../../mozilla.components.browser.menu.item/-browser-menu-compound-button/index.md)`, `[`HighlightableMenuItem`](./index.md)<br>A browser menu switch that can show a highlighted icon. |
