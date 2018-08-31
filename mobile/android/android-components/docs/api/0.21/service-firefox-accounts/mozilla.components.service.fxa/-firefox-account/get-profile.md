---
title: FirefoxAccount.getProfile - 
---

[mozilla.components.service.fxa](../index.html) / [FirefoxAccount](index.html) / [getProfile](./get-profile.html)

# getProfile

`fun getProfile(ignoreCache: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`FxaResult`](../-fxa-result/index.html)`<`[`Profile`](../-profile/index.html)`>`

Fetches the profile object for the current client either from the existing cached account,
or from the server (requires the client to have access to the profile scope).

### Parameters

`ignoreCache` - Fetch the profile information directly from the server

**Return**
FxaResult&lt;[Profile](../-profile/index.html)&gt; representing the user's basic profile info

`fun getProfile(): `[`FxaResult`](../-fxa-result/index.html)`<`[`Profile`](../-profile/index.html)`>`

Convenience method to fetch the profile from a cached account by default, but fall back
to retrieval from the server.

**Return**
FxaResult&lt;[Profile](../-profile/index.html)&gt; representing the user's basic profile info

