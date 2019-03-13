[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FirefoxAccount](index.md) / [getProfile](./get-profile.md)

# getProfile

`fun getProfile(ignoreCache: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): Deferred<`[`Profile`](../../mozilla.components.concept.sync/-profile/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FirefoxAccount.kt#L97)

Overrides [OAuthAccount.getProfile](../../mozilla.components.concept.sync/-o-auth-account/get-profile.md)

Fetches the profile object for the current client either from the existing cached account,
or from the server (requires the client to have access to the profile scope).

### Parameters

`ignoreCache` - Fetch the profile information directly from the server

### Exceptions

`Unauthorized` - We couldn't find any suitable access token to make that call.
The caller should then start the OAuth Flow again with the "profile" scope.

**Return**
Deferred&lt;[Profile](../../mozilla.components.concept.sync/-profile/index.md)&gt; representing the user's basic profile info

`fun getProfile(): Deferred<`[`Profile`](../../mozilla.components.concept.sync/-profile/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FirefoxAccount.kt#L111)

Overrides [OAuthAccount.getProfile](../../mozilla.components.concept.sync/-o-auth-account/get-profile.md)

Convenience method to fetch the profile from a cached account by default, but fall back
to retrieval from the server.

### Exceptions

`Unauthorized` - We couldn't find any suitable access token to make that call.
The caller should then start the OAuth Flow again with the "profile" scope.

**Return**
Deferred&lt;[Profile](../../mozilla.components.concept.sync/-profile/index.md)&gt; representing the user's basic profile info

