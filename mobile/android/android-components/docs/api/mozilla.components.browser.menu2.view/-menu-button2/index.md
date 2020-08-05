[android-components](../../index.md) / [mozilla.components.browser.menu2.view](../index.md) / [MenuButton2](./index.md)

# MenuButton2

`class MenuButton2 : `[`MenuButton`](../../mozilla.components.concept.menu/-menu-button/index.md)`, `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](../../mozilla.components.concept.menu/-menu-button/-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu2/src/main/java/mozilla/components/browser/menu2/view/MenuButton2.kt#L32)

A `three-dot` button used for expanding menus.

If you are using a browser toolbar, do not use this class directly.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `MenuButton2(context: <ERROR CLASS>, attrs: <ERROR CLASS>? = null, defStyleAttr: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0)`<br>A `three-dot` button used for expanding menus. |

### Properties

| Name | Summary |
|---|---|
| [menuController](menu-controller.md) | `var menuController: `[`MenuController`](../../mozilla.components.concept.menu/-menu-controller/index.md)`?`<br>Sets a [MenuController](../../mozilla.components.concept.menu/-menu-controller/index.md) that will be used to create a menu when this button is clicked. |

### Functions

| Name | Summary |
|---|---|
| [onClick](on-click.md) | `fun onClick(v: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Shows the menu. |
| [setColorFilter](set-color-filter.md) | `fun setColorFilter(color: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): <ERROR CLASS>`<br>Sets the tint of the 3-dot menu icon. |
| [setEffect](set-effect.md) | `fun setEffect(effect: `[`MenuEffect`](../../mozilla.components.concept.menu.candidate/-menu-effect.md)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Show the indicator for a browser menu effect. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
