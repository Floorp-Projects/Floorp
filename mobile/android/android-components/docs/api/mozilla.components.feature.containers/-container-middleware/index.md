[android-components](../../index.md) / [mozilla.components.feature.containers](../index.md) / [ContainerMiddleware](./index.md)

# ContainerMiddleware

`class ContainerMiddleware : `[`Middleware`](../../mozilla.components.lib.state/-middleware.md)`<`[`BrowserState`](../../mozilla.components.browser.state.state/-browser-state/index.md)`, `[`BrowserAction`](../../mozilla.components.browser.state.action/-browser-action.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/containers/src/main/java/mozilla/components/feature/containers/ContainerMiddleware.kt#L24)

[Middleware](../../mozilla.components.lib.state/-middleware.md) implementation for handling [ContainerAction](../../mozilla.components.browser.state.action/-container-action/index.md) and syncing the containers in
[BrowserState.containers](../../mozilla.components.browser.state.state/-browser-state/containers.md) with the [ContainerStorage](#).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ContainerMiddleware(applicationContext: <ERROR CLASS>, coroutineContext: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = Dispatchers.IO)`<br>[Middleware](../../mozilla.components.lib.state/-middleware.md) implementation for handling [ContainerAction](../../mozilla.components.browser.state.action/-container-action/index.md) and syncing the containers in [BrowserState.containers](../../mozilla.components.browser.state.state/-browser-state/containers.md) with the [ContainerStorage](#). |

### Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `fun invoke(store: `[`MiddlewareStore`](../../mozilla.components.lib.state/-middleware-store/index.md)`<`[`BrowserState`](../../mozilla.components.browser.state.state/-browser-state/index.md)`, `[`BrowserAction`](../../mozilla.components.browser.state.action/-browser-action.md)`>, next: (`[`BrowserAction`](../../mozilla.components.browser.state.action/-browser-action.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, action: `[`BrowserAction`](../../mozilla.components.browser.state.action/-browser-action.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
