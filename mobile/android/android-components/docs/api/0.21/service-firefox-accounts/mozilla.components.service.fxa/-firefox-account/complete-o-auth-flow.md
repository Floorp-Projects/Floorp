---
title: FirefoxAccount.completeOAuthFlow - 
---

[mozilla.components.service.fxa](../index.html) / [FirefoxAccount](index.html) / [completeOAuthFlow](./complete-o-auth-flow.html)

# completeOAuthFlow

`fun completeOAuthFlow(code: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, state: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`FxaResult`](../-fxa-result/index.html)`<`[`OAuthInfo`](../-o-auth-info/index.html)`>`

Authenticates the current account using the code and state parameters fetched from the
redirect URL reached after completing the sign in flow triggered by [beginOAuthFlow](begin-o-auth-flow.html).

Modifies the FirefoxAccount state.

