[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [OAuthAccount](index.md) / [getProfileAsync](./get-profile-async.md)

# getProfileAsync

`abstract fun getProfileAsync(ignoreCache: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): Deferred<`[`Profile`](../-profile/index.md)`?>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/OAuthAccount.kt#L127)

Fetches the profile object for the current client either from the existing cached state
or from the server (requires the client to have access to the profile scope).

### Parameters

`ignoreCache` - Fetch the profile information directly from the server

**Return**
Profile (optional, if successfully retrieved) representing the user's basic profile info

