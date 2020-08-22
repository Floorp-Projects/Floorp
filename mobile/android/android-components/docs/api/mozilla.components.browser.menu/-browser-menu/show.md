[android-components](../../index.md) / [mozilla.components.browser.menu](../index.md) / [BrowserMenu](index.md) / [show](./show.md)

# show

`open fun show(anchor: <ERROR CLASS>, orientation: `[`Orientation`](-orientation/index.md)` = DOWN, style: `[`MenuStyle`](../../mozilla.components.concept.menu/-menu-style/index.md)`? = null, endOfMenuAlwaysVisible: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, onDismiss: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {}): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/BrowserMenu.kt#L49)

### Parameters

`anchor` - the view on which to pin the popup window.

`orientation` - the preferred orientation to show the popup window.

`style` - Custom styling for this menu.

`endOfMenuAlwaysVisible` - when is set to true makes sure the bottom of the menu is always visible otherwise,
the top of the menu is always visible.