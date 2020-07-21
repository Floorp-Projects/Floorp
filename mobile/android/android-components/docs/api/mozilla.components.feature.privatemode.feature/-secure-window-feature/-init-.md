[android-components](../../index.md) / [mozilla.components.feature.privatemode.feature](../index.md) / [SecureWindowFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`SecureWindowFeature(window: <ERROR CLASS>, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, customTabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, isSecure: (`[`SessionState`](../../mozilla.components.browser.state.state/-session-state/index.md)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { it.content.private })`

Prevents screenshots and screen recordings in private tabs.

### Parameters

`isSecure` - Returns true if the session should have [FLAG_SECURE](#) set.
Can be overriden to customize when the secure flag is set.