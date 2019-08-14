[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FirefoxAccount](index.md) / [getProfileAsync](./get-profile-async.md)

# getProfileAsync

`fun getProfileAsync(ignoreCache: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): Deferred<`[`Profile`](../../mozilla.components.concept.sync/-profile/index.md)`?>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FirefoxAccount.kt#L131)

Overrides [OAuthAccount.getProfileAsync](../../mozilla.components.concept.sync/-o-auth-account/get-profile-async.md)

Fetches the profile object for the current client either from the existing cached account,
or from the server (requires the client to have access to the profile scope).

### Parameters

`ignoreCache` - Fetch the profile information directly from the server

**Return**
Profile (optional, if successfully retrieved) representing the user's basic profile info

`fun getProfileAsync(): Deferred<`[`Profile`](../../mozilla.components.concept.sync/-profile/index.md)`?>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FirefoxAccount.kt#L153)

Overrides [OAuthAccount.getProfileAsync](../../mozilla.components.concept.sync/-o-auth-account/get-profile-async.md)

Convenience method to fetch the profile from a cached account by default, but fall back
to retrieval from the server.

**Return**
Profile (optional, if successfully retrieved) representing the user's basic profile info

