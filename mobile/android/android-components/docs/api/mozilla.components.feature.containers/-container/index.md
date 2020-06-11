[android-components](../../index.md) / [mozilla.components.feature.containers](../index.md) / [Container](./index.md)

# Container

`interface Container` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/containers/src/main/java/mozilla/components/feature/containers/Container.kt#L10)

A container also known as a contextual identity.

### Properties

| Name | Summary |
|---|---|
| [color](color.md) | `abstract val color: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The color for the container. This can be shown in tabs belonging to this container. |
| [contextId](context-id.md) | `abstract val contextId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The session context ID also known as cookie store ID for the container. |
| [icon](icon.md) | `abstract val icon: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The name of an icon for the container. |
| [name](name.md) | `abstract val name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Name of the container. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
