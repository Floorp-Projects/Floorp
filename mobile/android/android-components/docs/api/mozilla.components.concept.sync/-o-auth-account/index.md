[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [OAuthAccount](./index.md)

# OAuthAccount

`interface OAuthAccount : `[`AutoCloseable`](https://developer.android.com/reference/java/lang/AutoCloseable.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/OAuthAccount.kt#L27)

Facilitates testing consumers of FirefoxAccount.

### Functions

| Name | Summary |
|---|---|
| [authInfo](auth-info.md) | `open suspend fun authInfo(singleScope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`AuthInfo`](../-auth-info/index.md)<br>Returns an [AuthInfo](../-auth-info/index.md) instance which may be used for data synchronization. |
| [beginOAuthFlowAsync](begin-o-auth-flow-async.md) | `abstract fun beginOAuthFlowAsync(scopes: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, wantsKeys: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): Deferred<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?>` |
| [beginPairingFlowAsync](begin-pairing-flow-async.md) | `abstract fun beginPairingFlowAsync(pairingUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, scopes: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): Deferred<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?>` |
| [checkAuthorizationStatusAsync](check-authorization-status-async.md) | `abstract fun checkAuthorizationStatusAsync(singleScope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`?>` |
| [completeOAuthFlowAsync](complete-o-auth-flow-async.md) | `abstract fun completeOAuthFlowAsync(code: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, state: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` |
| [deviceConstellation](device-constellation.md) | `abstract fun deviceConstellation(): `[`DeviceConstellation`](../-device-constellation/index.md) |
| [getAccessTokenAsync](get-access-token-async.md) | `abstract fun getAccessTokenAsync(singleScope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`AccessTokenInfo`](../-access-token-info/index.md)`?>` |
| [getProfileAsync](get-profile-async.md) | `abstract fun getProfileAsync(ignoreCache: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): Deferred<`[`Profile`](../-profile/index.md)`?>`<br>`abstract fun getProfileAsync(): Deferred<`[`Profile`](../-profile/index.md)`?>` |
| [getTokenServerEndpointURL](get-token-server-endpoint-u-r-l.md) | `abstract fun getTokenServerEndpointURL(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [registerPersistenceCallback](register-persistence-callback.md) | `abstract fun registerPersistenceCallback(callback: `[`StatePersistenceCallback`](../-state-persistence-callback/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [toJSONString](to-j-s-o-n-string.md) | `abstract fun toJSONString(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [FirefoxAccount](../../mozilla.components.service.fxa/-firefox-account/index.md) | `class FirefoxAccount : `[`OAuthAccount`](./index.md)<br>FirefoxAccount represents the authentication state of a client. |
