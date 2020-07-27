[android-components](../../index.md) / [mozilla.components.browser.menu2](../index.md) / [BrowserMenuController](./index.md)

# BrowserMenuController

`class BrowserMenuController : `[`MenuController`](../../mozilla.components.concept.menu/-menu-controller/index.md)`, `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](../../mozilla.components.concept.menu/-menu-controller/-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu2/src/main/java/mozilla/components/browser/menu2/BrowserMenuController.kt#L27)

Controls a popup menu composed of MenuCandidate objects.

### Parameters

`visibleSide` - Sets the menu to open with either the start or end visible.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserMenuController(visibleSide: `[`Side`](../../mozilla.components.concept.menu/-side/index.md)` = Side.START)`<br>Controls a popup menu composed of MenuCandidate objects. |

### Functions

| Name | Summary |
|---|---|
| [dismiss](dismiss.md) | `fun dismiss(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Dismiss the menu popup if the menu is visible. |
| [show](show.md) | `fun show(anchor: <ERROR CLASS>): <ERROR CLASS>`<br>`fun show(anchor: <ERROR CLASS>, orientation: `[`Orientation`](../../mozilla.components.concept.menu/-orientation/index.md)`? = null, width: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = anchor.resources.getDimensionPixelSize(R.dimen.mozac_browser_menu2_width)): <ERROR CLASS>` |
| [submitList](submit-list.md) | `fun submitList(list: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`MenuCandidate`](../../mozilla.components.concept.menu.candidate/-menu-candidate/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Changes the contents of the menu. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
