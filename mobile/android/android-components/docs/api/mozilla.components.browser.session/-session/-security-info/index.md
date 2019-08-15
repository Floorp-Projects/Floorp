[android-components](../../../index.md) / [mozilla.components.browser.session](../../index.md) / [Session](../index.md) / [SecurityInfo](./index.md)

# SecurityInfo

`data class SecurityInfo` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/Session.kt#L116)

A value type holding security information for a Session.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SecurityInfo(secure: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, host: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", issuer: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "")`<br>A value type holding security information for a Session. |

### Properties

| Name | Summary |
|---|---|
| [host](host.md) | `val host: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>domain for which the SSL certificate was issued. |
| [issuer](issuer.md) | `val issuer: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>name of the certificate authority who issued the SSL certificate. |
| [secure](secure.md) | `val secure: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>true if the session is currently pointed to a URL with a valid SSL certificate, otherwise false. |

### Extension Functions

| Name | Summary |
|---|---|
| [toSecurityInfoState](../../../mozilla.components.browser.session.ext/to-security-info-state.md) | `fun `[`SecurityInfo`](./index.md)`.toSecurityInfoState(): `[`SecurityInfoState`](../../../mozilla.components.browser.state.state/-security-info-state/index.md)<br>Creates a matching [SecurityInfoState](../../../mozilla.components.browser.state.state/-security-info-state/index.md) from a [Session.SecurityInfo](./index.md) |
