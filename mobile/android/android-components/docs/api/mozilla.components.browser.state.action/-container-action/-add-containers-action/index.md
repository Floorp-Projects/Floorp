[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [ContainerAction](../index.md) / [AddContainersAction](./index.md)

# AddContainersAction

`data class AddContainersAction : `[`ContainerAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L607)

Updates [BrowserState.containers](../../../mozilla.components.browser.state.state/-browser-state/containers.md) to register the given list of [containers](containers.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AddContainersAction(containers: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`ContainerState`](../../../mozilla.components.browser.state.state/-container-state/index.md)`>)`<br>Updates [BrowserState.containers](../../../mozilla.components.browser.state.state/-browser-state/containers.md) to register the given list of [containers](containers.md). |

### Properties

| Name | Summary |
|---|---|
| [containers](containers.md) | `val containers: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`ContainerState`](../../../mozilla.components.browser.state.state/-container-state/index.md)`>` |
