[android-components](../../index.md) / [mozilla.components.browser.menu](../index.md) / [WebExtensionBrowserMenu](index.md) / [show](./show.md)

# show

`@ExperimentalCoroutinesApi fun show(anchor: <ERROR CLASS>, orientation: `[`Orientation`](../-browser-menu/-orientation/index.md)`, style: `[`MenuStyle`](../../mozilla.components.concept.menu/-menu-style/index.md)`?, endOfMenuAlwaysVisible: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, onDismiss: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/WebExtensionBrowserMenu.kt#L34)

Overrides [BrowserMenu.show](../-browser-menu/show.md)

### Parameters

`anchor` - the view on which to pin the popup window.

`orientation` - the preferred orientation to show the popup window.

`style` - Custom styling for this menu.

`endOfMenuAlwaysVisible` - when is set to true makes sure the bottom of the menu is always visible otherwise,
the top of the menu is always visible.