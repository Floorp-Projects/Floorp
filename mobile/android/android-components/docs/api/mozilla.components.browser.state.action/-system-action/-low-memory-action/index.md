[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [SystemAction](../index.md) / [LowMemoryAction](./index.md)

# LowMemoryAction

`data class LowMemoryAction : `[`SystemAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L56)

Optimizes the [BrowserState](../../../mozilla.components.browser.state.state/-browser-state/index.md) by removing unneeded and optional resources if the system is in
a low memory condition.

### Parameters

`level` - The context of the trim, giving a hint of the amount of trimming the application
may like to perform. See constants in [ComponentCallbacks2](#).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `LowMemoryAction(level: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`)`<br>Optimizes the [BrowserState](../../../mozilla.components.browser.state.state/-browser-state/index.md) by removing unneeded and optional resources if the system is in a low memory condition. |

### Properties

| Name | Summary |
|---|---|
| [level](level.md) | `val level: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>The context of the trim, giving a hint of the amount of trimming the application may like to perform. See constants in [ComponentCallbacks2](#). |
