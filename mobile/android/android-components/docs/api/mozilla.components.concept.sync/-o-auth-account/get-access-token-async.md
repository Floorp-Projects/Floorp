[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [OAuthAccount](index.md) / [getAccessTokenAsync](./get-access-token-async.md)

# getAccessTokenAsync

`abstract fun getAccessTokenAsync(singleScope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`AccessTokenInfo`](../-access-token-info/index.md)`?>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/OAuthAccount.kt#L147)

Tries to fetch an access token for the given scope.

### Parameters

`singleScope` - Single OAuth scope (no spaces) for which the client wants access

**Return**
[AccessTokenInfo](../-access-token-info/index.md) that stores the token, along with its scope, key and
    expiration timestamp (in seconds) since epoch when complete

