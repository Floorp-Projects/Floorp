[android-components](../../index.md) / [mozilla.components.browser.state.store](../index.md) / [BrowserStore](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`BrowserStore(initialState: `[`BrowserState`](../../mozilla.components.browser.state.state/-browser-state/index.md)` = BrowserState(), middleware: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Middleware`](../../mozilla.components.lib.state/-middleware.md)`<`[`BrowserState`](../../mozilla.components.browser.state.state/-browser-state/index.md)`, `[`BrowserAction`](../../mozilla.components.browser.state.action/-browser-action.md)`>> = emptyList())`

The [BrowserStore](index.md) holds the [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md) (state tree).

The only way to change the [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md) inside [BrowserStore](index.md) is to dispatch an [Action](../../mozilla.components.lib.state/-action.md) on it.

