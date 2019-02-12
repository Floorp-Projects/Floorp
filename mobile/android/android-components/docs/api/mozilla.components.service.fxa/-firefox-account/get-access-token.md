[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FirefoxAccount](index.md) / [getAccessToken](./get-access-token.md)

# getAccessToken

`fun getAccessToken(singleScope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`AccessTokenInfo`](../-access-token-info/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FirefoxAccount.kt#L125)

Overrides [FirefoxAccountShaped.getAccessToken](../-firefox-account-shaped/get-access-token.md)

Tries to fetch an access token for the given scope.

### Parameters

`scope` - Single OAuth scope (no spaces) for which the client wants access

### Exceptions

`Unauthorized` - We couldn't provide an access token for this scope.
The caller should then start the OAuth Flow again with the desired scope.

**Return**
[AccessTokenInfo](../-access-token-info/index.md) that stores the token, along with its scope, key and
    expiration timestamp (in seconds) since epoch when complete

