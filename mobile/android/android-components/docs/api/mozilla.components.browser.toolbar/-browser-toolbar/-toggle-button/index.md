[android-components](../../../index.md) / [mozilla.components.browser.toolbar](../../index.md) / [BrowserToolbar](../index.md) / [ToggleButton](./index.md)

# ToggleButton

`class ToggleButton : `[`ActionToggleButton`](../../../mozilla.components.concept.toolbar/-toolbar/-action-toggle-button/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/toolbar/src/main/java/mozilla/components/browser/toolbar/BrowserToolbar.kt#L741)

An action button with two states, selected and unselected. When the button is pressed, the
state changes automatically.

### Parameters

`image` - The drawable to be shown if the button is in unselected state.

`imageSelected` - The drawable to be shown if the button is in selected state.

`contentDescription` - The content description to use if the button is in unselected state.

`contentDescriptionSelected` - The content description to use if the button is in selected state.

`visible` - Lambda that returns true or false to indicate whether this button should be shown.

`selected` - Sets whether this button should be selected initially.

`background` - A custom (stateful) background drawable resource to be used.

`padding` - a custom [Padding](../../../mozilla.components.support.base.android/-padding/index.md) for this Button.

`listener` - Callback that will be invoked whenever the checked state changes.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ToggleButton(image: <ERROR CLASS>, imageSelected: <ERROR CLASS>, contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, contentDescriptionSelected: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true }, selected: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, background: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0, padding: `[`Padding`](../../../mozilla.components.support.base.android/-padding/index.md)` = DEFAULT_PADDING, listener: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>An action button with two states, selected and unselected. When the button is pressed, the state changes automatically. |

### Properties

| Name | Summary |
|---|---|
| [padding](padding.md) | `val padding: `[`Padding`](../../../mozilla.components.support.base.android/-padding/index.md)<br>a custom [Padding](../../../mozilla.components.support.base.android/-padding/index.md) for this Button. |

### Inherited Properties

| Name | Summary |
|---|---|
| [visible](../../../mozilla.components.concept.toolbar/-toolbar/-action-toggle-button/visible.md) | `open val visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Lambda that returns true or false to indicate whether this button should be shown. |

### Inherited Functions

| Name | Summary |
|---|---|
| [bind](../../../mozilla.components.concept.toolbar/-toolbar/-action-toggle-button/bind.md) | `open fun bind(view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [createView](../../../mozilla.components.concept.toolbar/-toolbar/-action-toggle-button/create-view.md) | `open fun createView(parent: <ERROR CLASS>): <ERROR CLASS>` |
| [isSelected](../../../mozilla.components.concept.toolbar/-toolbar/-action-toggle-button/is-selected.md) | `fun isSelected(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns the current selected state of the action. |
| [setSelected](../../../mozilla.components.concept.toolbar/-toolbar/-action-toggle-button/set-selected.md) | `fun setSelected(selected: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, notifyListener: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Changes the selected state of the action. |
| [toggle](../../../mozilla.components.concept.toolbar/-toolbar/-action-toggle-button/toggle.md) | `fun toggle(notifyListener: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Changes the selected state of the action to the inverse of its current state. |
