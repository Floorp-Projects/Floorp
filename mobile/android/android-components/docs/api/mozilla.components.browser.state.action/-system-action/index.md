[android-components](../../index.md) / [mozilla.components.browser.state.action](../index.md) / [SystemAction](./index.md)

# SystemAction

`sealed class SystemAction : `[`BrowserAction`](../-browser-action.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L33)

[BrowserAction](../-browser-action.md) implementations to react to system events.

### Types

| Name | Summary |
|---|---|
| [LowMemoryAction](-low-memory-action.md) | `object LowMemoryAction : `[`SystemAction`](./index.md)<br>Optimizes the [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md) by removing unneeded and optional resources if the system is in a low memory condition. |

### Inheritors

| Name | Summary |
|---|---|
| [LowMemoryAction](-low-memory-action.md) | `object LowMemoryAction : `[`SystemAction`](./index.md)<br>Optimizes the [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md) by removing unneeded and optional resources if the system is in a low memory condition. |
