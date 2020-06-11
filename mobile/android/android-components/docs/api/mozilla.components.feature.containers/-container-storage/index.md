[android-components](../../index.md) / [mozilla.components.feature.containers](../index.md) / [ContainerStorage](./index.md)

# ContainerStorage

`class ContainerStorage` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/containers/src/main/java/mozilla/components/feature/containers/ContainerStorage.kt#L20)

A storage implementation for organizing containers (contextual identities).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ContainerStorage(context: <ERROR CLASS>)`<br>A storage implementation for organizing containers (contextual identities). |

### Functions

| Name | Summary |
|---|---|
| [addContainer](add-container.md) | `suspend fun addContainer(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, color: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, icon: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds a new [Container](../-container/index.md). |
| [getContainers](get-containers.md) | `fun getContainers(): Flow<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Container`](../-container/index.md)`>>`<br>Returns a [Flow](#) list of all the [Container](../-container/index.md) instances. |
| [getContainersPaged](get-containers-paged.md) | `fun getContainersPaged(): Factory<`[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, `[`Container`](../-container/index.md)`>`<br>Returns all saved [Container](../-container/index.md) instances as a [DataSource.Factory](#). |
| [removeContainer](remove-container.md) | `suspend fun removeContainer(container: `[`Container`](../-container/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes the given [Container](../-container/index.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
