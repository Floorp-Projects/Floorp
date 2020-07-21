[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [SyncAuthInfo](./index.md)

# SyncAuthInfo

`data class SyncAuthInfo` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Sync.kt#L37)

A Firefox Sync friendly auth object which can be obtained from [OAuthAccount](../-o-auth-account/index.md).

Why is there a Firefox Sync-shaped authentication object at the concept level, you ask?
Mainly because this is what the [SyncableStore](../-syncable-store/index.md) consumes in order to actually perform
synchronization, which is in turn implemented by `places`-backed storage layer.
If this class lived in `services-firefox-accounts`, we'd end up with an ugly dependency situation
between services and storage components.

Turns out that building a generic description of an authentication/synchronization layer is not
quite the way to go when you only have a single, legacy implementation.

However, this may actually improve once we retire the tokenserver from the architecture.
We could also consider a heavier use of generics, as well.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SyncAuthInfo(kid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, fxaAccessToken: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, fxaAccessTokenExpiresAt: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, syncKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, tokenServerUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>A Firefox Sync friendly auth object which can be obtained from [OAuthAccount](../-o-auth-account/index.md). |

### Properties

| Name | Summary |
|---|---|
| [fxaAccessToken](fxa-access-token.md) | `val fxaAccessToken: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [fxaAccessTokenExpiresAt](fxa-access-token-expires-at.md) | `val fxaAccessTokenExpiresAt: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |
| [kid](kid.md) | `val kid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [syncKey](sync-key.md) | `val syncKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [tokenServerUrl](token-server-url.md) | `val tokenServerUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [into](../../mozilla.components.service.sync.logins/into.md) | `fun `[`SyncAuthInfo`](./index.md)`.into(): `[`SyncUnlockInfo`](../../mozilla.components.service.sync.logins/-sync-unlock-info.md)<br>Conversion from a generic AuthInfo type into a type 'logins' lib uses at the interface boundary. |
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
