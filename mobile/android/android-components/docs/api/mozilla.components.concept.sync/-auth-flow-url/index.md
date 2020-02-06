[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [AuthFlowUrl](./index.md)

# AuthFlowUrl

`data class AuthFlowUrl` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/OAuthAccount.kt#L43)

An object that represents a login flow initiated by [OAuthAccount](../-o-auth-account/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AuthFlowUrl(state: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>An object that represents a login flow initiated by [OAuthAccount](../-o-auth-account/index.md). |

### Properties

| Name | Summary |
|---|---|
| [state](state.md) | `val state: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>OAuth state parameter, identifying a specific authentication flow. This string is randomly generated during [OAuthAccount.beginOAuthFlowAsync](../-o-auth-account/begin-o-auth-flow-async.md) and [OAuthAccount.beginPairingFlowAsync](../-o-auth-account/begin-pairing-flow-async.md). |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Url which needs to be loaded to go through the authentication flow identified by [state](state.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
