[android-components](../../../index.md) / [mozilla.components.concept.toolbar](../../index.md) / [Toolbar](../index.md) / [Action](./index.md)

# Action

`interface Action` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/toolbar/src/main/java/mozilla/components/concept/toolbar/Toolbar.kt#L163)

Generic interface for actions to be added to the toolbar.

### Properties

| Name | Summary |
|---|---|
| [visible](visible.md) | `open val visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Functions

| Name | Summary |
|---|---|
| [bind](bind.md) | `abstract fun bind(view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [createView](create-view.md) | `abstract fun createView(parent: <ERROR CLASS>): <ERROR CLASS>` |

### Inheritors

| Name | Summary |
|---|---|
| [ActionButton](../-action-button/index.md) | `open class ActionButton : `[`Action`](./index.md)<br>An action button to be added to the toolbar. |
| [ActionImage](../-action-image/index.md) | `open class ActionImage : `[`Action`](./index.md)<br>An action that just shows a static, non-clickable image. |
| [ActionSpace](../-action-space/index.md) | `open class ActionSpace : `[`Action`](./index.md)<br>An "empty" action with a desired width to be used as "placeholder". |
| [ActionToggleButton](../-action-toggle-button/index.md) | `open class ActionToggleButton : `[`Action`](./index.md)<br>An action button with two states, selected and unselected. When the button is pressed, the state changes automatically. |
| [TabCounterToolbarButton](../../../mozilla.components.feature.tabs.toolbar/-tab-counter-toolbar-button/index.md) | `class TabCounterToolbarButton : `[`Action`](./index.md)<br>A [Toolbar.Action](./index.md) implementation that shows a [TabCounter](../../../mozilla.components.ui.tabcounter/-tab-counter/index.md). |
