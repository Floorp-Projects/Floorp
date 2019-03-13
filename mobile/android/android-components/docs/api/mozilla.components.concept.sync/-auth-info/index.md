[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [AuthInfo](./index.md)

# AuthInfo

`data class AuthInfo` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/OAuthAccount.kt#L47)

A Firefox Sync friendly auth object which can be obtained from [OAuthAccount](../-o-auth-account/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AuthInfo(kid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, fxaAccessToken: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, syncKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, tokenServerUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>A Firefox Sync friendly auth object which can be obtained from [OAuthAccount](../-o-auth-account/index.md). |

### Properties

| Name | Summary |
|---|---|
| [fxaAccessToken](fxa-access-token.md) | `val fxaAccessToken: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [kid](kid.md) | `val kid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [syncKey](sync-key.md) | `val syncKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [tokenServerUrl](token-server-url.md) | `val tokenServerUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [into](../../mozilla.components.service.sync.logins/into.md) | `fun `[`AuthInfo`](./index.md)`.into(): `[`SyncUnlockInfo`](../../mozilla.components.service.sync.logins/-sync-unlock-info.md)<br>Conversion from a generic AuthInfo type into a type 'logins' lib uses at the interface boundary. |
