[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FirefoxAccount](./index.md)

# FirefoxAccount

`class FirefoxAccount : `[`OAuthAccount`](../../mozilla.components.concept.sync/-o-auth-account/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FirefoxAccount.kt#L26)

FirefoxAccount represents the authentication state of a client.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FirefoxAccount(config: `[`Config`](../-config.md)`, persistCallback: `[`PersistCallback`](../-persist-callback.md)`? = null)`<br>Construct a FirefoxAccount from a [Config](../-config.md), a clientId, and a redirectUri. |

### Functions

| Name | Summary |
|---|---|
| [beginOAuthFlow](begin-o-auth-flow.md) | `fun beginOAuthFlow(scopes: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, wantsKeys: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): Deferred<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>Constructs a URL used to begin the OAuth flow for the requested scopes and keys. |
| [beginPairingFlow](begin-pairing-flow.md) | `fun beginPairingFlow(pairingUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, scopes: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): Deferred<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` |
| [close](close.md) | `fun close(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [completeOAuthFlow](complete-o-auth-flow.md) | `fun completeOAuthFlow(code: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, state: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>Authenticates the current account using the code and state parameters fetched from the redirect URL reached after completing the sign in flow triggered by [beginOAuthFlow](begin-o-auth-flow.md). |
| [getAccessToken](get-access-token.md) | `fun getAccessToken(singleScope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`AccessTokenInfo`](../../mozilla.components.concept.sync/-access-token-info/index.md)`>`<br>Tries to fetch an access token for the given scope. |
| [getConnectionSuccessURL](get-connection-success-u-r-l.md) | `fun getConnectionSuccessURL(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Fetches the connection success url. |
| [getProfile](get-profile.md) | `fun getProfile(ignoreCache: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): Deferred<`[`Profile`](../../mozilla.components.concept.sync/-profile/index.md)`>`<br>Fetches the profile object for the current client either from the existing cached account, or from the server (requires the client to have access to the profile scope).`fun getProfile(): Deferred<`[`Profile`](../../mozilla.components.concept.sync/-profile/index.md)`>`<br>Convenience method to fetch the profile from a cached account by default, but fall back to retrieval from the server. |
| [getTokenServerEndpointURL](get-token-server-endpoint-u-r-l.md) | `fun getTokenServerEndpointURL(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Fetches the token server endpoint, for authentication using the SAML bearer flow. |
| [registerPersistCallback](register-persist-callback.md) | `fun registerPersistCallback(persistCallback: `[`PersistCallback`](../-persist-callback.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers a [PersistCallback](../-persist-callback.md) that will be called every time the [FirefoxAccount](./index.md) internal state has mutated. The [FirefoxAccount](./index.md) instance can be later restored using the [FirefoxAccount.fromJSONString](from-j-s-o-n-string.md) class method. It is the responsibility of the consumer to ensure the persisted data is saved in a secure location, as it can contain Sync Keys and OAuth tokens. |
| [toJSONString](to-j-s-o-n-string.md) | `fun toJSONString(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Saves the current account's authentication state as a JSON string, for persistence in the Android KeyStore/shared preferences. The authentication state can be restored using [FirefoxAccount.fromJSONString](from-j-s-o-n-string.md). |
| [unregisterPersistCallback](unregister-persist-callback.md) | `fun unregisterPersistCallback(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Unregisters any previously registered [PersistCallback](../-persist-callback.md). |

### Inherited Functions

| Name | Summary |
|---|---|
| [authInfo](../../mozilla.components.concept.sync/-o-auth-account/auth-info.md) | `open suspend fun authInfo(singleScope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`AuthInfo`](../../mozilla.components.concept.sync/-auth-info/index.md) |

### Companion Object Functions

| Name | Summary |
|---|---|
| [fromJSONString](from-j-s-o-n-string.md) | `fun fromJSONString(json: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, persistCallback: `[`PersistCallback`](../-persist-callback.md)`? = null): `[`FirefoxAccount`](./index.md)<br>Restores the account's authentication state from a JSON string produced by [FirefoxAccount.toJSONString](to-j-s-o-n-string.md). |
