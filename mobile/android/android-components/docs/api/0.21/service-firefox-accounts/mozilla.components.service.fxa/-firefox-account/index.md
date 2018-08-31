---
title: FirefoxAccount - 
---

[mozilla.components.service.fxa](../index.html) / [FirefoxAccount](./index.html)

# FirefoxAccount

`class FirefoxAccount : `[`RustObject`](../-rust-object/index.html)`<`[`RawFxAccount`](../-raw-fx-account/index.html)`>`

FirefoxAccount represents the authentication state of a client.

**Parameters**

### Constructors

| [&lt;init&gt;](-init-.html) | `FirefoxAccount(config: `[`Config`](../-config/index.html)`, clientId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, redirectUri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)``FirefoxAccount(rawPointer: `[`RawFxAccount`](../-raw-fx-account/index.html)`?)`<br>FirefoxAccount represents the authentication state of a client. |

### Properties

| [rawPointer](raw-pointer.html) | `var rawPointer: `[`RawFxAccount`](../-raw-fx-account/index.html)`?` |

### Inherited Properties

| [isConsumed](../-rust-object/is-consumed.html) | `val isConsumed: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Functions

| [beginOAuthFlow](begin-o-auth-flow.html) | `fun beginOAuthFlow(scopes: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, wantsKeys: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`FxaResult`](../-fxa-result/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>Constructs a URL used to begin the OAuth flow for the requested scopes and keys. |
| [completeOAuthFlow](complete-o-auth-flow.html) | `fun completeOAuthFlow(code: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, state: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`FxaResult`](../-fxa-result/index.html)`<`[`OAuthInfo`](../-o-auth-info/index.html)`>`<br>Authenticates the current account using the code and state parameters fetched from the redirect URL reached after completing the sign in flow triggered by [beginOAuthFlow](begin-o-auth-flow.html). |
| [destroy](destroy.html) | `fun destroy(p: `[`RawFxAccount`](../-raw-fx-account/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [getOAuthToken](get-o-auth-token.html) | `fun getOAuthToken(scopes: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`FxaResult`](../-fxa-result/index.html)`<`[`OAuthInfo`](../-o-auth-info/index.html)`>`<br>Fetches a new access token for the desired scopes using an internally stored refresh token. |
| [getProfile](get-profile.html) | `fun getProfile(ignoreCache: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`FxaResult`](../-fxa-result/index.html)`<`[`Profile`](../-profile/index.html)`>`<br>Fetches the profile object for the current client either from the existing cached account, or from the server (requires the client to have access to the profile scope).`fun getProfile(): `[`FxaResult`](../-fxa-result/index.html)`<`[`Profile`](../-profile/index.html)`>`<br>Convenience method to fetch the profile from a cached account by default, but fall back to retrieval from the server. |
| [getSyncKeys](get-sync-keys.html) | `fun getSyncKeys(): `[`SyncKeys`](../-sync-keys/index.html)<br>Fetches keys for encryption/decryption of Firefox Sync data. |
| [getTokenServerEndpointURL](get-token-server-endpoint-u-r-l.html) | `fun getTokenServerEndpointURL(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Fetches the token server endpoint, for authentication using the SAML bearer flow. |
| [newAssertion](new-assertion.html) | `fun newAssertion(audience: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Creates a new SAML assertion from the account state, which can be posted to the token server endpoint fetched from [getTokenServerEndpointURL](get-token-server-endpoint-u-r-l.html) in order to get an access token. |
| [toJSONString](to-j-s-o-n-string.html) | `fun toJSONString(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Saves the current account's authentication state as a JSON string, for persistence in the Android KeyStore/shared preferences. The authentication state can be restored using [FirefoxAccount.fromJSONString](from-j-s-o-n-string.html). |

### Inherited Functions

| [close](../-rust-object/close.html) | `open fun close(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [consumePointer](../-rust-object/consume-pointer.html) | `fun consumePointer(): `[`T`](../-rust-object/index.html#T) |
| [validPointer](../-rust-object/valid-pointer.html) | `fun validPointer(): `[`T`](../-rust-object/index.html#T) |

### Companion Object Functions

| [from](from.html) | `fun from(config: `[`Config`](../-config/index.html)`, clientId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, redirectUri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, webChannelResponse: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`FxaResult`](../-fxa-result/index.html)`<`[`FirefoxAccount`](./index.md)`>` |
| [fromJSONString](from-j-s-o-n-string.html) | `fun fromJSONString(json: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`FxaResult`](../-fxa-result/index.html)`<`[`FirefoxAccount`](./index.md)`>`<br>Restores the account's authentication state from a JSON string produced by [FirefoxAccount.toJSONString](to-j-s-o-n-string.html). |

