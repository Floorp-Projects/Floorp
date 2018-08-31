---
title: FirefoxAccount.getOAuthToken - 
---

[mozilla.components.service.fxa](../index.html) / [FirefoxAccount](index.html) / [getOAuthToken](./get-o-auth-token.html)

# getOAuthToken

`fun getOAuthToken(scopes: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`FxaResult`](../-fxa-result/index.html)`<`[`OAuthInfo`](../-o-auth-info/index.html)`>`

Fetches a new access token for the desired scopes using an internally stored refresh token.

### Parameters

`scopes` - List of OAuth scopes for which the client wants access

### Exceptions

`FxaException.Unauthorized` - if the token could not be retrieved (eg. expired refresh token)

**Return**
FxaResult&lt;[OAuthInfo](../-o-auth-info/index.html)&gt; that stores the token, along with its scopes and keys when complete

