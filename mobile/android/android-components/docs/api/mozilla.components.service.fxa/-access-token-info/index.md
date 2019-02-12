[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [AccessTokenInfo](./index.md)

# AccessTokenInfo

`data class AccessTokenInfo` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/AccessTokenInfo.kt#L28)

The result of authentication with FxA via an OAuth flow.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AccessTokenInfo(token: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, key: `[`OAuthScopedKey`](../-o-auth-scoped-key/index.md)`?, expiresAt: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`)`<br>The result of authentication with FxA via an OAuth flow. |

### Properties

| Name | Summary |
|---|---|
| [expiresAt](expires-at.md) | `val expiresAt: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>The expiry date timestamp of this token since unix epoch (in seconds). |
| [key](key.md) | `val key: `[`OAuthScopedKey`](../-o-auth-scoped-key/index.md)`?`<br>An OAuthScopedKey if present. |
| [token](token.md) | `val token: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The access token produced by the flow. |
