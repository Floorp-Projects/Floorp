[android-components](../../index.md) / [mozilla.components.browser.menu](../index.md) / [BrowserMenu](./index.md)

# BrowserMenu

`class BrowserMenu` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/BrowserMenu.kt#L30)

A popup menu composed of BrowserMenuItem objects.

### Types

| Name | Summary |
|---|---|
| [Orientation](-orientation/index.md) | `enum class Orientation` |

### Functions

| Name | Summary |
|---|---|
| [dismiss](dismiss.md) | `fun dismiss(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [invalidate](invalidate.md) | `fun invalidate(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [show](show.md) | `fun show(anchor: <ERROR CLASS>, orientation: `[`Orientation`](-orientation/index.md)` = DOWN, endOfMenuAlwaysVisible: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, onDismiss: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {}): <ERROR CLASS>` |

### Companion Object Functions

| Name | Summary |
|---|---|
| [determineMenuOrientation](determine-menu-orientation.md) | `fun determineMenuOrientation(parent: <ERROR CLASS>?): `[`Orientation`](-orientation/index.md)<br>Determines the orientation to be used for a menu based on the positioning of the [parent](determine-menu-orientation.md#mozilla.components.browser.menu.BrowserMenu.Companion$determineMenuOrientation()/parent) in the layout. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
