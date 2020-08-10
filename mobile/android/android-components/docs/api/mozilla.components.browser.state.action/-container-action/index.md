[android-components](../../index.md) / [mozilla.components.browser.state.action](../index.md) / [ContainerAction](./index.md)

# ContainerAction

`sealed class ContainerAction : `[`BrowserAction`](../-browser-action.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L598)

[BrowserAction](../-browser-action.md) implementations related to updating [BrowserState.containers](../../mozilla.components.browser.state.state/-browser-state/containers.md)

### Types

| Name | Summary |
|---|---|
| [AddContainerAction](-add-container-action/index.md) | `data class AddContainerAction : `[`ContainerAction`](./index.md)<br>Updates [BrowserState.containers](../../mozilla.components.browser.state.state/-browser-state/containers.md) to register the given added [container](-add-container-action/container.md). |
| [AddContainersAction](-add-containers-action/index.md) | `data class AddContainersAction : `[`ContainerAction`](./index.md)<br>Updates [BrowserState.containers](../../mozilla.components.browser.state.state/-browser-state/containers.md) to register the given list of [containers](-add-containers-action/containers.md). |
| [InitializeContainerState](-initialize-container-state.md) | `object InitializeContainerState : `[`ContainerAction`](./index.md)<br>Initializes the [BrowserState.containers](../../mozilla.components.browser.state.state/-browser-state/containers.md) state. |
| [RemoveContainerAction](-remove-container-action/index.md) | `data class RemoveContainerAction : `[`ContainerAction`](./index.md)<br>Removes all state of the removed container from [BrowserState.containers](../../mozilla.components.browser.state.state/-browser-state/containers.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [AddContainerAction](-add-container-action/index.md) | `data class AddContainerAction : `[`ContainerAction`](./index.md)<br>Updates [BrowserState.containers](../../mozilla.components.browser.state.state/-browser-state/containers.md) to register the given added [container](-add-container-action/container.md). |
| [AddContainersAction](-add-containers-action/index.md) | `data class AddContainersAction : `[`ContainerAction`](./index.md)<br>Updates [BrowserState.containers](../../mozilla.components.browser.state.state/-browser-state/containers.md) to register the given list of [containers](-add-containers-action/containers.md). |
| [InitializeContainerState](-initialize-container-state.md) | `object InitializeContainerState : `[`ContainerAction`](./index.md)<br>Initializes the [BrowserState.containers](../../mozilla.components.browser.state.state/-browser-state/containers.md) state. |
| [RemoveContainerAction](-remove-container-action/index.md) | `data class RemoveContainerAction : `[`ContainerAction`](./index.md)<br>Removes all state of the removed container from [BrowserState.containers](../../mozilla.components.browser.state.state/-browser-state/containers.md). |
