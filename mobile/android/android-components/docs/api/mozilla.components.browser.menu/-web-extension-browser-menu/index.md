[android-components](../../index.md) / [mozilla.components.browser.menu](../index.md) / [WebExtensionBrowserMenu](./index.md)

# WebExtensionBrowserMenu

`class WebExtensionBrowserMenu : `[`BrowserMenu`](../-browser-menu/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/WebExtensionBrowserMenu.kt#L26)

A [BrowserMenu](../-browser-menu/index.md) capable of displaying browser and page actions from web extensions.

### Inherited Properties

| Name | Summary |
|---|---|
| [currentPopup](../-browser-menu/current-popup.md) | `var currentPopup: <ERROR CLASS>?` |

### Functions

| Name | Summary |
|---|---|
| [show](show.md) | `fun show(anchor: <ERROR CLASS>, orientation: `[`Orientation`](../-browser-menu/-orientation/index.md)`, endOfMenuAlwaysVisible: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, onDismiss: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): <ERROR CLASS>` |

### Inherited Functions

| Name | Summary |
|---|---|
| [dismiss](../-browser-menu/dismiss.md) | `fun dismiss(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [invalidate](../-browser-menu/invalidate.md) | `fun invalidate(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onViewAttachedToWindow](../-browser-menu/on-view-attached-to-window.md) | `open fun onViewAttachedToWindow(v: <ERROR CLASS>?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onViewDetachedFromWindow](../-browser-menu/on-view-detached-from-window.md) | `open fun onViewDetachedFromWindow(v: <ERROR CLASS>?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
