[android-components](../../index.md) / [mozilla.components.browser.menu.item](../index.md) / [BrowserMenuDivider](./index.md)

# BrowserMenuDivider

`class BrowserMenuDivider : `[`BrowserMenuItem`](../../mozilla.components.browser.menu/-browser-menu-item/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/item/BrowserMenuDivider.kt#L18)

A browser menu item to display a horizontal divider.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserMenuDivider()`<br>A browser menu item to display a horizontal divider. |

### Properties

| Name | Summary |
|---|---|
| [interactiveCount](interactive-count.md) | `val interactiveCount: () -> `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Lambda expression that returns the number of interactive elements in this menu item. For example, a simple item will have 1, divider will have 0, and a composite item, like a tool bar, will have several. |
| [visible](visible.md) | `var visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Lambda expression that returns true if this item should be shown in the menu. Returns false if this item should be hidden. |

### Functions

| Name | Summary |
|---|---|
| [asCandidate](as-candidate.md) | `fun asCandidate(context: <ERROR CLASS>): `[`DividerMenuCandidate`](../../mozilla.components.concept.menu.candidate/-divider-menu-candidate/index.md)<br>Converts the menu item into a menu candidate. |
| [bind](bind.md) | `fun bind(menu: `[`BrowserMenu`](../../mozilla.components.browser.menu/-browser-menu/index.md)`, view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to display the data of this item using the passed view. |
| [getLayoutResource](get-layout-resource.md) | `fun getLayoutResource(): <ERROR CLASS>`<br>Returns the layout resource ID of the layout to be inflated for showing a menu item of this type. |

### Inherited Functions

| Name | Summary |
|---|---|
| [invalidate](../../mozilla.components.browser.menu/-browser-menu-item/invalidate.md) | `open fun invalidate(view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to update the displayed data of this item using the passed view. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
