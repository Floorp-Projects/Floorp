[android-components](../../index.md) / [mozilla.components.browser.state.state](../index.md) / [SecurityInfoState](./index.md)

# SecurityInfoState

`data class SecurityInfoState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/SecurityInfoState.kt#L15)

A value type holding security information for a Session.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SecurityInfoState(secure: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, host: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", issuer: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "")`<br>A value type holding security information for a Session. |

### Properties

| Name | Summary |
|---|---|
| [host](host.md) | `val host: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>domain for which the SSL certificate was issued. |
| [issuer](issuer.md) | `val issuer: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>name of the certificate authority who issued the SSL certificate. |
| [secure](secure.md) | `val secure: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>true if the tab is currently pointed to a URL with a valid SSL certificate, otherwise false. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
