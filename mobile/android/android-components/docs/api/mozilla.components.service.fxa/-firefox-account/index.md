[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FirefoxAccount](./index.md)

# FirefoxAccount

`class FirefoxAccount : `[`OAuthAccount`](../../mozilla.components.concept.sync/-o-auth-account/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FirefoxAccount.kt#L28)

FirefoxAccount represents the authentication state of a client.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FirefoxAccount(config: `[`ServerConfig`](../-server-config.md)`, persistCallback: `[`PersistCallback`](../-persist-callback.md)`? = null)`<br>Construct a FirefoxAccount from a [Config](#), a clientId, and a redirectUri. |

### Functions

| Name | Summary |
|---|---|
| [beginOAuthFlowAsync](begin-o-auth-flow-async.md) | `fun beginOAuthFlowAsync(scopes: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, wantsKeys: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): Deferred<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?>`<br>Constructs a URL used to begin the OAuth flow for the requested scopes and keys. |
| [beginPairingFlowAsync](begin-pairing-flow-async.md) | `fun beginPairingFlowAsync(pairingUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, scopes: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): Deferred<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?>` |
| [checkAuthorizationStatusAsync](check-authorization-status-async.md) | `fun checkAuthorizationStatusAsync(singleScope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`?>`<br>This method should be called when a request made with an OAuth token failed with an authentication error. It will re-build cached state and perform a connectivity check. |
| [close](close.md) | `fun close(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [completeOAuthFlowAsync](complete-o-auth-flow-async.md) | `fun completeOAuthFlowAsync(code: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, state: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>Authenticates the current account using the code and state parameters fetched from the redirect URL reached after completing the sign in flow triggered by [beginOAuthFlowAsync](begin-o-auth-flow-async.md). |
| [deviceConstellation](device-constellation.md) | `fun deviceConstellation(): `[`DeviceConstellation`](../../mozilla.components.concept.sync/-device-constellation/index.md) |
| [disconnectAsync](disconnect-async.md) | `fun disconnectAsync(): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>Reset internal account state and destroy current device record. Use this when device record is no longer relevant, e.g. while logging out. On success, other devices will no longer see the current device in their device lists. |
| [getAccessTokenAsync](get-access-token-async.md) | `fun getAccessTokenAsync(singleScope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`AccessTokenInfo`](../../mozilla.components.concept.sync/-access-token-info/index.md)`?>`<br>Tries to fetch an access token for the given scope. |
| [getConnectionSuccessURL](get-connection-success-u-r-l.md) | `fun getConnectionSuccessURL(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Fetches the connection success url. |
| [getProfileAsync](get-profile-async.md) | `fun getProfileAsync(ignoreCache: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): Deferred<`[`Profile`](../../mozilla.components.concept.sync/-profile/index.md)`?>`<br>Fetches the profile object for the current client either from the existing cached account, or from the server (requires the client to have access to the profile scope).`fun getProfileAsync(): Deferred<`[`Profile`](../../mozilla.components.concept.sync/-profile/index.md)`?>`<br>Convenience method to fetch the profile from a cached account by default, but fall back to retrieval from the server. |
| [getTokenServerEndpointURL](get-token-server-endpoint-u-r-l.md) | `fun getTokenServerEndpointURL(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Fetches the token server endpoint, for authentication using the SAML bearer flow. |
| [migrateFromSessionTokenAsync](migrate-from-session-token-async.md) | `fun migrateFromSessionTokenAsync(sessionToken: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, kSync: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, kXCS: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` |
| [registerPersistenceCallback](register-persistence-callback.md) | `fun registerPersistenceCallback(callback: `[`StatePersistenceCallback`](../../mozilla.components.concept.sync/-state-persistence-callback/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [toJSONString](to-j-s-o-n-string.md) | `fun toJSONString(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Saves the current account's authentication state as a JSON string, for persistence in the Android KeyStore/shared preferences. The authentication state can be restored using [FirefoxAccount.fromJSONString](from-j-s-o-n-string.md). |

### Companion Object Functions

| Name | Summary |
|---|---|
| [fromJSONString](from-j-s-o-n-string.md) | `fun fromJSONString(json: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, persistCallback: `[`PersistCallback`](../-persist-callback.md)`? = null): `[`FirefoxAccount`](./index.md)<br>Restores the account's authentication state from a JSON string produced by [FirefoxAccount.toJSONString](to-j-s-o-n-string.md). |
