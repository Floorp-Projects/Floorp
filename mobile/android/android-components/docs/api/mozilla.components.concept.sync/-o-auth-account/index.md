[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [OAuthAccount](./index.md)

# OAuthAccount

`interface OAuthAccount : `[`AutoCloseable`](https://developer.android.com/reference/java/lang/AutoCloseable.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/OAuthAccount.kt#L20)

Facilitates testing consumers of FirefoxAccount.

### Functions

| Name | Summary |
|---|---|
| [authInfo](auth-info.md) | `open suspend fun authInfo(singleScope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`AuthInfo`](../-auth-info/index.md) |
| [beginOAuthFlow](begin-o-auth-flow.md) | `abstract fun beginOAuthFlow(scopes: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, wantsKeys: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): Deferred<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` |
| [beginPairingFlow](begin-pairing-flow.md) | `abstract fun beginPairingFlow(pairingUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, scopes: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): Deferred<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` |
| [completeOAuthFlow](complete-o-auth-flow.md) | `abstract fun completeOAuthFlow(code: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, state: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` |
| [getAccessToken](get-access-token.md) | `abstract fun getAccessToken(singleScope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`AccessTokenInfo`](../-access-token-info/index.md)`>` |
| [getProfile](get-profile.md) | `abstract fun getProfile(ignoreCache: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): Deferred<`[`Profile`](../-profile/index.md)`>`<br>`abstract fun getProfile(): Deferred<`[`Profile`](../-profile/index.md)`>` |
| [getTokenServerEndpointURL](get-token-server-endpoint-u-r-l.md) | `abstract fun getTokenServerEndpointURL(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [toJSONString](to-j-s-o-n-string.md) | `abstract fun toJSONString(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [FirefoxAccount](../../mozilla.components.service.fxa/-firefox-account/index.md) | `class FirefoxAccount : `[`OAuthAccount`](./index.md)<br>FirefoxAccount represents the authentication state of a client. |
