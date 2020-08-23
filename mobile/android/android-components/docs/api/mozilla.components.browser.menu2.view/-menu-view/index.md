[android-components](../../index.md) / [mozilla.components.browser.menu2.view](../index.md) / [MenuView](./index.md)

# MenuView

`class MenuView` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu2/src/main/java/mozilla/components/browser/menu2/view/MenuView.kt#L29)

A popup menu composed of [MenuCandidate](../../mozilla.components.concept.menu.candidate/-menu-candidate/index.md) objects.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `MenuView(context: <ERROR CLASS>, attrs: <ERROR CLASS>? = null, defStyleAttr: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0)`<br>A popup menu composed of [MenuCandidate](../../mozilla.components.concept.menu.candidate/-menu-candidate/index.md) objects. |

### Properties

| Name | Summary |
|---|---|
| [onDismiss](on-dismiss.md) | `var onDismiss: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called when the menu is clicked and should be dismissed. |
| [onReopenMenu](on-reopen-menu.md) | `var onReopenMenu: (`[`NestedMenuCandidate`](../../mozilla.components.concept.menu.candidate/-nested-menu-candidate/index.md)`?) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called when a nested menu should be opened. |

### Functions

| Name | Summary |
|---|---|
| [setStyle](set-style.md) | `fun setStyle(style: `[`MenuStyle`](../../mozilla.components.concept.menu/-menu-style/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the background color for the menu view. |
| [setVisibleSide](set-visible-side.md) | `fun setVisibleSide(side: `[`Side`](../../mozilla.components.concept.menu/-side/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Displays either the start or the end of the list. |
| [submitList](submit-list.md) | `fun submitList(list: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`MenuCandidate`](../../mozilla.components.concept.menu.candidate/-menu-candidate/index.md)`>?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Changes the contents of the menu. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
