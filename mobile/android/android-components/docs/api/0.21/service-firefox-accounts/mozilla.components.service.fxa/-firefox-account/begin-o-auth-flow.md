---
title: FirefoxAccount.beginOAuthFlow - 
---

[mozilla.components.service.fxa](../index.html) / [FirefoxAccount](index.html) / [beginOAuthFlow](./begin-o-auth-flow.html)

# beginOAuthFlow

`fun beginOAuthFlow(scopes: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, wantsKeys: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`FxaResult`](../-fxa-result/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`

Constructs a URL used to begin the OAuth flow for the requested scopes and keys.

### Parameters

`scopes` - List of OAuth scopes for which the client wants access

`wantsKeys` - Fetch keys for end-to-end encryption of data from Mozilla-hosted services

**Return**
FxaResult that resolves to the flow URL when complete

