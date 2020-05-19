[android-components](../../index.md) / [mozilla.components.browser.menu.item](../index.md) / [BrowserMenuHighlightableSwitch](./index.md)

# BrowserMenuHighlightableSwitch

`class BrowserMenuHighlightableSwitch : `[`BrowserMenuCompoundButton`](../-browser-menu-compound-button/index.md)`, `[`HighlightableMenuItem`](../../mozilla.components.browser.menu/-highlightable-menu-item/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/item/BrowserMenuHighlightableSwitch.kt#L30)

A browser menu switch that can show a highlighted icon.

### Parameters

`label` - The visible label of this menu item.

`initialState` - The initial value the checkbox should have.

`listener` - Callback to be invoked when this menu item is checked.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserMenuHighlightableSwitch(label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, startImageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, iconTintColorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, textColorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, highlight: `[`LowPriority`](../../mozilla.components.browser.menu/-browser-menu-highlight/-low-priority/index.md)`, isHighlighted: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true }, initialState: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { false }, listener: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>A browser menu switch that can show a highlighted icon. |

### Properties

| Name | Summary |
|---|---|
| [highlight](highlight.md) | `val highlight: `[`LowPriority`](../../mozilla.components.browser.menu/-browser-menu-highlight/-low-priority/index.md)<br>Highlight object representing how the menu item will be displayed when highlighted. |
| [isHighlighted](is-highlighted.md) | `val isHighlighted: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Whether or not to display the highlight |

### Inherited Properties

| Name | Summary |
|---|---|
| [label](../-browser-menu-compound-button/label.md) | `val label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The visible label of this menu item. |
| [visible](../-browser-menu-compound-button/visible.md) | `open var visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Lambda expression that returns true if this item should be shown in the menu. Returns false if this item should be hidden. |

### Functions

| Name | Summary |
|---|---|
| [asCandidate](as-candidate.md) | `fun asCandidate(context: <ERROR CLASS>): `[`CompoundMenuCandidate`](../../mozilla.components.concept.menu.candidate/-compound-menu-candidate/index.md)<br>Converts the menu item into a menu candidate. |
| [bind](bind.md) | `fun bind(menu: `[`BrowserMenu`](../../mozilla.components.browser.menu/-browser-menu/index.md)`, view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to display the data of this item using the passed view. |
| [getLayoutResource](get-layout-resource.md) | `fun getLayoutResource(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Returns the layout resource ID of the layout to be inflated for showing a menu item of this type. |
| [invalidate](invalidate.md) | `fun invalidate(view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to update the displayed data of this item using the passed view. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
