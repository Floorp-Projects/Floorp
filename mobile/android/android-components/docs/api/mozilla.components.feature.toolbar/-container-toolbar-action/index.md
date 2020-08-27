[android-components](../../index.md) / [mozilla.components.feature.toolbar](../index.md) / [ContainerToolbarAction](./index.md)

# ContainerToolbarAction

`class ContainerToolbarAction : `[`Action`](../../mozilla.components.concept.toolbar/-toolbar/-action/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/toolbar/src/main/java/mozilla/components/feature/toolbar/ContainerToolbarAction.kt#L30)

An action button that represents a container to be added to the toolbar.

### Parameters

`container` - Associated [ContainerState](../../mozilla.components.browser.state.state/-container-state/index.md)'s icon and color to render in the toolbar.

`padding` - A optional custom padding.

`listener` - A optional callback that will be invoked whenever the button is pressed.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ContainerToolbarAction(container: `[`ContainerState`](../../mozilla.components.browser.state.state/-container-state/index.md)`, padding: `[`Padding`](../../mozilla.components.support.base.android/-padding/index.md)`? = null, listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null)`<br>An action button that represents a container to be added to the toolbar. |

### Inherited Properties

| Name | Summary |
|---|---|
| [visible](../../mozilla.components.concept.toolbar/-toolbar/-action/visible.md) | `open val visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Functions

| Name | Summary |
|---|---|
| [bind](bind.md) | `fun bind(view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [createView](create-view.md) | `fun createView(parent: <ERROR CLASS>): <ERROR CLASS>` |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
