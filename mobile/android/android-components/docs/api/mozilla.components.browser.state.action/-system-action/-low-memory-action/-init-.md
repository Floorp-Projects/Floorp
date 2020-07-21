[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [SystemAction](../index.md) / [LowMemoryAction](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`LowMemoryAction(states: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`EngineSessionState`](../../../mozilla.components.concept.engine/-engine-session-state/index.md)`>)`

Optimizes the [BrowserState](../../../mozilla.components.browser.state.state/-browser-state/index.md) by removing unneeded and optional
resources if the system is in a low memory condition.

### Parameters

`states` - map of session ids to engine session states where the engine session was closed
by SessionManager.