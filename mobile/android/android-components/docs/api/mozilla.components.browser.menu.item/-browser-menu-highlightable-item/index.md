[android-components](../../index.md) / [mozilla.components.browser.menu.item](../index.md) / [BrowserMenuHighlightableItem](./index.md)

# BrowserMenuHighlightableItem

`class BrowserMenuHighlightableItem : `[`BrowserMenuImageText`](../-browser-menu-image-text/index.md)`, `[`HighlightableMenuItem`](../../mozilla.components.browser.menu/-highlightable-menu-item/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/item/BrowserMenuHighlightableItem.kt#L38)

A menu item for displaying text with an image icon and a highlight state which sets the
background of the menu item and a second image icon to the right of the text.

### Parameters

`label` - The default visible label of this menu item.

`startImageResource` - ID of a drawable resource to be shown as a leftmost icon.

`iconTintColorResource` - Optional ID of color resource to tint the icon.

`textColorResource` - Optional ID of color resource to tint the text.

`highlight` - Highlight object representing how the menu item will be displayed when highlighted.

`isHighlighted` - Whether or not to display the highlight

`listener` - Callback to be invoked when this menu item is clicked.

### Types

| Name | Summary |
|---|---|
| [Highlight](-highlight/index.md) | `class ~~Highlight~~ : `[`ClassicHighlight`](../../mozilla.components.browser.menu/-browser-menu-highlight/-classic-highlight/index.md)<br>Described how to display a [BrowserMenuHighlightableItem](./index.md) when it is highlighted. Replaced by [BrowserMenuHighlight](../../mozilla.components.browser.menu/-browser-menu-highlight/index.md) which lets a priority be specified. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserMenuHighlightableItem(label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, imageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, iconTintColorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, textColorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, highlight: `[`Highlight`](-highlight/index.md)`? = null, listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {})``BrowserMenuHighlightableItem(label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, startImageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, iconTintColorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, textColorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, highlight: `[`BrowserMenuHighlight`](../../mozilla.components.browser.menu/-browser-menu-highlight/index.md)`, isHighlighted: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true }, listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {})`<br>A menu item for displaying text with an image icon and a highlight state which sets the background of the menu item and a second image icon to the right of the text. |

### Properties

| Name | Summary |
|---|---|
| [highlight](highlight.md) | `val highlight: `[`BrowserMenuHighlight`](../../mozilla.components.browser.menu/-browser-menu-highlight/index.md)<br>Highlight object representing how the menu item will be displayed when highlighted. |
| [isHighlighted](is-highlighted.md) | `val isHighlighted: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Whether or not to display the highlight |

### Inherited Properties

| Name | Summary |
|---|---|
| [visible](../-browser-menu-image-text/visible.md) | `open var visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Lambda expression that returns true if this item should be shown in the menu. Returns false if this item should be hidden. |

### Functions

| Name | Summary |
|---|---|
| [asCandidate](as-candidate.md) | `fun asCandidate(context: <ERROR CLASS>): `[`TextMenuCandidate`](../../mozilla.components.concept.menu.candidate/-text-menu-candidate/index.md)<br>Converts the menu item into a menu candidate. |
| [bind](bind.md) | `fun bind(menu: `[`BrowserMenu`](../../mozilla.components.browser.menu/-browser-menu/index.md)`, view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to display the data of this item using the passed view. |
| [getLayoutResource](get-layout-resource.md) | `fun getLayoutResource(): <ERROR CLASS>`<br>Returns the layout resource ID of the layout to be inflated for showing a menu item of this type. |
| [invalidate](invalidate.md) | `fun invalidate(view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to update the displayed data of this item using the passed view. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
