[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FirefoxAccountShaped](./index.md)

# FirefoxAccountShaped

`interface FirefoxAccountShaped : `[`AutoCloseable`](https://developer.android.com/reference/java/lang/AutoCloseable.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FirefoxAccount.kt#L19)

Facilitates testing consumers of FirefoxAccount.

### Functions

| Name | Summary |
|---|---|
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
| [FirefoxAccount](../-firefox-account/index.md) | `class FirefoxAccount : `[`FirefoxAccountShaped`](./index.md)<br>FirefoxAccount represents the authentication state of a client. |
