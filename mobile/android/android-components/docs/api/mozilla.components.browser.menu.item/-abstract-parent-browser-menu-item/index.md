[android-components](../../index.md) / [mozilla.components.browser.menu.item](../index.md) / [AbstractParentBrowserMenuItem](./index.md)

# AbstractParentBrowserMenuItem

`abstract class AbstractParentBrowserMenuItem : `[`BrowserMenuItem`](../../mozilla.components.browser.menu/-browser-menu-item/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/item/AbstractParentBrowserMenuItem.kt#L19)

An abstract menu item for handling nested sub menu items on view click.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AbstractParentBrowserMenuItem(subMenu: `[`BrowserMenu`](../../mozilla.components.browser.menu/-browser-menu/index.md)`, endOfMenuAlwaysVisible: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`)`<br>An abstract menu item for handling nested sub menu items on view click. |

### Properties

| Name | Summary |
|---|---|
| [onSubMenuDismiss](on-sub-menu-dismiss.md) | `var onSubMenuDismiss: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Listener called when the sub menu is dismissed. |
| [onSubMenuShow](on-sub-menu-show.md) | `var onSubMenuShow: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Listener called when the sub menu is shown. |
| [visible](visible.md) | `abstract var visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Lambda expression that returns true if this item should be shown in the menu. Returns false if this item should be hidden. |

### Inherited Properties

| Name | Summary |
|---|---|
| [interactiveCount](../../mozilla.components.browser.menu/-browser-menu-item/interactive-count.md) | `open val interactiveCount: () -> `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Lambda expression that returns the number of interactive elements in this menu item. For example, a simple item will have 1, divider will have 0, and a composite item, like a tool bar, will have several. |

### Functions

| Name | Summary |
|---|---|
| [bind](bind.md) | `open fun bind(menu: `[`BrowserMenu`](../../mozilla.components.browser.menu/-browser-menu/index.md)`, view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to display the data of this item using the passed view. |
| [getLayoutResource](get-layout-resource.md) | `abstract fun getLayoutResource(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Returns the layout resource ID of the layout to be inflated for showing a menu item of this type. |

### Inherited Functions

| Name | Summary |
|---|---|
| [asCandidate](../../mozilla.components.browser.menu/-browser-menu-item/as-candidate.md) | `open fun asCandidate(context: <ERROR CLASS>): `[`MenuCandidate`](../../mozilla.components.concept.menu.candidate/-menu-candidate/index.md)`?`<br>Converts the menu item into a menu candidate. |
| [invalidate](../../mozilla.components.browser.menu/-browser-menu-item/invalidate.md) | `open fun invalidate(view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the browser menu to update the displayed data of this item using the passed view. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [ParentBrowserMenuItem](../-parent-browser-menu-item/index.md) | `class ParentBrowserMenuItem : `[`AbstractParentBrowserMenuItem`](./index.md)<br>A parent menu item for displaying text and an image icon with a nested sub menu. It handles back pressing if the sub menu contains a [BackPressMenuItem](../-back-press-menu-item/index.md). |
