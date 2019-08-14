[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FirefoxAccount](index.md) / [getAccessTokenAsync](./get-access-token-async.md)

# getAccessTokenAsync

`fun getAccessTokenAsync(singleScope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`AccessTokenInfo`](../../mozilla.components.concept.sync/-access-token-info/index.md)`?>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FirefoxAccount.kt#L190)

Overrides [OAuthAccount.getAccessTokenAsync](../../mozilla.components.concept.sync/-o-auth-account/get-access-token-async.md)

Tries to fetch an access token for the given scope.

### Parameters

`singleScope` - Single OAuth scope (no spaces) for which the client wants access

**Return**
[AccessTokenInfo](../../mozilla.components.concept.sync/-access-token-info/index.md) that stores the token, along with its scope, key and
    expiration timestamp (in seconds) since epoch when complete

