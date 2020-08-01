[android-components](../../index.md) / [mozilla.components.feature.privatemode.feature](../index.md) / [SecureWindowFeature](./index.md)

# SecureWindowFeature

`class SecureWindowFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/privatemode/src/main/java/mozilla/components/feature/privatemode/feature/SecureWindowFeature.kt#L28)

Prevents screenshots and screen recordings in private tabs.

### Parameters

`isSecure` - Returns true if the session should have [FLAG_SECURE](#) set.
Can be overriden to customize when the secure flag is set.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SecureWindowFeature(window: <ERROR CLASS>, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, customTabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, isSecure: (`[`SessionState`](../../mozilla.components.browser.state.state/-session-state/index.md)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { it.content.private })`<br>Prevents screenshots and screen recordings in private tabs. |

### Functions

| Name | Summary |
|---|---|
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
