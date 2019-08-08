[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [AccessTokenInfo](./index.md)

# AccessTokenInfo

`data class AccessTokenInfo` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/OAuthAccount.kt#L113)

The result of authentication with FxA via an OAuth flow.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AccessTokenInfo(scope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, token: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, key: `[`OAuthScopedKey`](../-o-auth-scoped-key/index.md)`?, expiresAt: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`)`<br>The result of authentication with FxA via an OAuth flow. |

### Properties

| Name | Summary |
|---|---|
| [expiresAt](expires-at.md) | `val expiresAt: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>The expiry date timestamp of this token since unix epoch (in seconds). |
| [key](key.md) | `val key: `[`OAuthScopedKey`](../-o-auth-scoped-key/index.md)`?`<br>An OAuthScopedKey if present. |
| [scope](scope.md) | `val scope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [token](token.md) | `val token: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The access token produced by the flow. |

### Extension Functions

| Name | Summary |
|---|---|
| [asSyncAuthInfo](../../mozilla.components.service.fxa/as-sync-auth-info.md) | `fun `[`AccessTokenInfo`](./index.md)`.asSyncAuthInfo(tokenServerUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`SyncAuthInfo`](../-sync-auth-info/index.md)<br>Converts a generic [AccessTokenInfo](#) into a Firefox Sync-friendly [SyncAuthInfo](../-sync-auth-info/index.md) instance which may be used for data synchronization. |
