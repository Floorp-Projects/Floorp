[android-components](../../index.md) / [mozilla.components.browser.menu.item](../index.md) / [SimpleBrowserMenuHighlightableItem](./index.md)

# SimpleBrowserMenuHighlightableItem

`class SimpleBrowserMenuHighlightableItem : `[`BrowserMenuItem`](../../mozilla.components.browser.menu/-browser-menu-item/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/item/SimpleBrowserMenuHighlightableItem.kt#L34)

A menu item for displaying text with a highlight state which sets the
background of the menu item.

### Parameters

`label` - The default visible label of this menu item.

`textColorResource` - Optional ID of color resource to tint the text.

`textSize` - The size of the label.

`backgroundTint` - Tint for the menu item background color

`isHighlighted` - Whether or not to display the highlight

`listener` - Callback to be invoked when this menu item is clicked.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SimpleBrowserMenuHighlightableItem(label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, textColorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, textSize: `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html)` = NO_ID.toFloat(), backgroundTint: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, isHighlighted: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true }, listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {})`<br>A menu item for displaying text with a highlight state which sets the background of the menu item. |

### Properties

| Name | Summary |
|---|---|
| [backgroundTint](background-tint.md) | `val backgroundTint: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Tint for the menu item background color |
| [isHighlighted](is-highlighted.md) | `var isHighlighted: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Whether or not to display the highlight |
| [visible](visible.md) | `var visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Lambda expression that returns true if this item should be shown in the menu. Returns false if this item should be hidden. |

### Inherited Properties

| Name | Summary |
|---|---|
| [interactiveCount](../../mozilla.components.browser.menu/-browser-menu-item/interactive-count.md) | `open val interactiveCount: () -> `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Lambda expression that returns the number of interactive elements in this menu item. For example, a simple item will have 1, divider will have 0, and a composite item, like a tool bar, will have several. |

### Functions

| Name | Summary |
|---|---|
| [asCandidate](as-candidate.md) | `fun asCandidate(context: <ERROR CLASS>): `[`MenuCandidate`](../../mozilla.components.concept.menu.candidate/-menu-candidate/index.md)<br>Converts the menu item into a menu candidate. |
| [bind](bind.md) | `fun bind(menu: `[`BrowserMenu`](../../mozilla.components.browser.menu/-browser-menu/index.md)`, view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to display the data of this item using the passed view. |
| [getLayoutResource](get-layout-resource.md) | `fun getLayoutResource(): <ERROR CLASS>`<br>Returns the layout resource ID of the layout to be inflated for showing a menu item of this type. |
| [invalidate](invalidate.md) | `fun invalidate(view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to update the displayed data of this item using the passed view. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
