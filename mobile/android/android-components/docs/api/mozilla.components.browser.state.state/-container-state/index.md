[android-components](../../index.md) / [mozilla.components.browser.state.state](../index.md) / [ContainerState](./index.md)

# ContainerState

`data class ContainerState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/ContainerState.kt#L15)

Value type that represents the state of a container also known as a contextual identity.

### Types

| Name | Summary |
|---|---|
| [Color](-color/index.md) | `enum class Color`<br>Enum of container color. |
| [Icon](-icon/index.md) | `enum class Icon`<br>Enum of container icon. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ContainerState(contextId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, color: `[`Color`](-color/index.md)`, icon: `[`Icon`](-icon/index.md)`)`<br>Value type that represents the state of a container also known as a contextual identity. |

### Properties

| Name | Summary |
|---|---|
| [color](color.md) | `val color: `[`Color`](-color/index.md)<br>The color for the container. This can be shown in tabs belonging to this container. |
| [contextId](context-id.md) | `val contextId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The session context ID also known as cookie store ID for the container. |
| [icon](icon.md) | `val icon: `[`Icon`](-icon/index.md)<br>The icon for the container. |
| [name](name.md) | `val name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Name of the container. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
