[android-components](../../../index.md) / [mozilla.components.concept.toolbar](../../index.md) / [Toolbar](../index.md) / [ActionToggleButton](./index.md)

# ActionToggleButton

`open class ActionToggleButton : `[`Action`](../-action/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/toolbar/src/main/java/mozilla/components/concept/toolbar/Toolbar.kt#L230)

An action button with two states, selected and unselected. When the button is pressed, the
state changes automatically.

### Parameters

`imageDrawable` - The drawable to be shown if the button is in unselected state.

`imageSelectedDrawable` - The  drawable to be shown if the button is in selected state.

`contentDescription` - The content description to use if the button is in unselected state.

`contentDescriptionSelected` - The content description to use if the button is in selected state.

`visible` - Lambda that returns true or false to indicate whether this button should be shown.

`selected` - Sets whether this button should be selected initially.

`padding` - A optional custom padding.

`listener` - Callback that will be invoked whenever the checked state changes.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ActionToggleButton(imageDrawable: <ERROR CLASS>, imageSelectedDrawable: <ERROR CLASS>, contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, contentDescriptionSelected: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true }, selected: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, background: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0, padding: `[`Padding`](../../../mozilla.components.support.base.android/-padding/index.md)`? = null, listener: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>An action button with two states, selected and unselected. When the button is pressed, the state changes automatically. |

### Properties

| Name | Summary |
|---|---|
| [visible](visible.md) | `open val visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Lambda that returns true or false to indicate whether this button should be shown. |

### Functions

| Name | Summary |
|---|---|
| [bind](bind.md) | `open fun bind(view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [createView](create-view.md) | `open fun createView(parent: <ERROR CLASS>): <ERROR CLASS>` |
| [isSelected](is-selected.md) | `fun isSelected(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns the current selected state of the action. |
| [setSelected](set-selected.md) | `fun setSelected(selected: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, notifyListener: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Changes the selected state of the action. |
| [toggle](toggle.md) | `fun toggle(notifyListener: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Changes the selected state of the action to the inverse of its current state. |

### Inheritors

| Name | Summary |
|---|---|
| [ToggleButton](../../../mozilla.components.browser.toolbar/-browser-toolbar/-toggle-button/index.md) | `class ToggleButton : `[`ActionToggleButton`](./index.md)<br>An action button with two states, selected and unselected. When the button is pressed, the state changes automatically. |
