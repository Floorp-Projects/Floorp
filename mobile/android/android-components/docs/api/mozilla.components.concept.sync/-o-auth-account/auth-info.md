[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [OAuthAccount](index.md) / [authInfo](./auth-info.md)

# authInfo

`open suspend fun authInfo(singleScope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`AuthInfo`](../-auth-info/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/OAuthAccount.kt#L45)

Returns an [AuthInfo](../-auth-info/index.md) instance which may be used for data synchronization.

### Exceptions

`AuthException` - if account needs to restart the OAuth flow.

**Return**
An [AuthInfo](../-auth-info/index.md) which is guaranteed to have a sync key.

