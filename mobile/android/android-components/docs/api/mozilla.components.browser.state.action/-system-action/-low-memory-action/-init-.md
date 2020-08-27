[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [SystemAction](../index.md) / [LowMemoryAction](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`LowMemoryAction(level: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`)`

Optimizes the [BrowserState](../../../mozilla.components.browser.state.state/-browser-state/index.md) by removing unneeded and optional resources if the system is in
a low memory condition.

### Parameters

`level` - The context of the trim, giving a hint of the amount of trimming the application
may like to perform. See constants in [ComponentCallbacks2](#).