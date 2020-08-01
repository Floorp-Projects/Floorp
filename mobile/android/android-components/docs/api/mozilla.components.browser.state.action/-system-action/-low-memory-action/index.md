[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [SystemAction](../index.md) / [LowMemoryAction](./index.md)

# LowMemoryAction

`data class LowMemoryAction : `[`SystemAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L54)

Optimizes the [BrowserState](../../../mozilla.components.browser.state.state/-browser-state/index.md) by removing unneeded and optional
resources if the system is in a low memory condition.

### Parameters

`states` - map of session ids to engine session states where the engine session was closed
by SessionManager.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `LowMemoryAction(states: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`EngineSessionState`](../../../mozilla.components.concept.engine/-engine-session-state/index.md)`>)`<br>Optimizes the [BrowserState](../../../mozilla.components.browser.state.state/-browser-state/index.md) by removing unneeded and optional resources if the system is in a low memory condition. |

### Properties

| Name | Summary |
|---|---|
| [states](states.md) | `val states: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`EngineSessionState`](../../../mozilla.components.concept.engine/-engine-session-state/index.md)`>`<br>map of session ids to engine session states where the engine session was closed by SessionManager. |
