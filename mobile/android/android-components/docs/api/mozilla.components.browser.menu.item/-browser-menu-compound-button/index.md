[android-components](../../index.md) / [mozilla.components.browser.menu.item](../index.md) / [BrowserMenuCompoundButton](./index.md)

# BrowserMenuCompoundButton

`abstract class BrowserMenuCompoundButton : `[`BrowserMenuItem`](../../mozilla.components.browser.menu/-browser-menu-item/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/item/BrowserMenuCompoundButton.kt#L25)

A browser menu compound button. A basic sub-class would only have to provide a layout resource to
satisfy [BrowserMenuItem.getLayoutResource](../../mozilla.components.browser.menu/-browser-menu-item/get-layout-resource.md) which contains a [View](#) that inherits from [CompoundButton](#).

### Parameters

`label` - The visible label of this menu item.

`initialState` - The initial value the checkbox should have.

`listener` - Callback to be invoked when this menu item is checked.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserMenuCompoundButton(label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, initialState: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { false }, listener: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>A browser menu compound button. A basic sub-class would only have to provide a layout resource to satisfy [BrowserMenuItem.getLayoutResource](../../mozilla.components.browser.menu/-browser-menu-item/get-layout-resource.md) which contains a [View](#) that inherits from [CompoundButton](#). |

### Properties

| Name | Summary |
|---|---|
| [label](label.md) | `val label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The visible label of this menu item. |
| [visible](visible.md) | `open var visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Lambda expression that returns true if this item should be shown in the menu. Returns false if this item should be hidden. |

### Inherited Properties

| Name | Summary |
|---|---|
| [interactiveCount](../../mozilla.components.browser.menu/-browser-menu-item/interactive-count.md) | `open val interactiveCount: () -> `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Lambda expression that returns the number of interactive elements in this menu item. For example, a simple item will have 1, divider will have 0, and a composite item, like a tool bar, will have several. |

### Functions

| Name | Summary |
|---|---|
| [asCandidate](as-candidate.md) | `open fun asCandidate(context: <ERROR CLASS>): `[`CompoundMenuCandidate`](../../mozilla.components.concept.menu.candidate/-compound-menu-candidate/index.md)<br>Converts the menu item into a menu candidate. |
| [bind](bind.md) | `open fun bind(menu: `[`BrowserMenu`](../../mozilla.components.browser.menu/-browser-menu/index.md)`, view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to display the data of this item using the passed view. |

### Inherited Functions

| Name | Summary |
|---|---|
| [getLayoutResource](../../mozilla.components.browser.menu/-browser-menu-item/get-layout-resource.md) | `abstract fun getLayoutResource(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Returns the layout resource ID of the layout to be inflated for showing a menu item of this type. |
| [invalidate](../../mozilla.components.browser.menu/-browser-menu-item/invalidate.md) | `open fun invalidate(view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to update the displayed data of this item using the passed view. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [BrowserMenuCheckbox](../-browser-menu-checkbox/index.md) | `class BrowserMenuCheckbox : `[`BrowserMenuCompoundButton`](./index.md)<br>A simple browser menu checkbox. |
| [BrowserMenuHighlightableSwitch](../-browser-menu-highlightable-switch/index.md) | `class BrowserMenuHighlightableSwitch : `[`BrowserMenuCompoundButton`](./index.md)`, `[`HighlightableMenuItem`](../../mozilla.components.browser.menu/-highlightable-menu-item/index.md)<br>A browser menu switch that can show a highlighted icon. |
| [BrowserMenuImageSwitch](../-browser-menu-image-switch/index.md) | `class BrowserMenuImageSwitch : `[`BrowserMenuCompoundButton`](./index.md)<br>A simple browser menu switch. |
| [BrowserMenuSwitch](../-browser-menu-switch/index.md) | `class BrowserMenuSwitch : `[`BrowserMenuCompoundButton`](./index.md)<br>A simple browser menu switch. |
