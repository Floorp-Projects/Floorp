[android-components](../../index.md) / [mozilla.components.browser.menu](../index.md) / [BrowserMenu](index.md) / [show](./show.md)

# show

`fun show(anchor: `[`View`](https://developer.android.com/reference/android/view/View.html)`, orientation: `[`Orientation`](-orientation/index.md)` = DOWN, endOfMenuAlwaysVisible: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, onDismiss: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {}): `[`PopupWindow`](https://developer.android.com/reference/android/widget/PopupWindow.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/BrowserMenu.kt#L42)

### Parameters

`anchor` - the view on which to pin the popup window.

`orientation` - the preferred orientation to show the popup window.

`endOfMenuAlwaysVisible` - when is set to true makes sure the bottom of the menu is always visible otherwise,
the top of the menu is always visible.