---
title: FirefoxAccount.newAssertion - 
---

[mozilla.components.service.fxa](../index.html) / [FirefoxAccount](index.html) / [newAssertion](./new-assertion.html)

# newAssertion

`fun newAssertion(audience: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`

Creates a new SAML assertion from the account state, which can be posted to the token server
endpoint fetched from [getTokenServerEndpointURL](get-token-server-endpoint-u-r-l.html) in order to get an access token.

**Return**
String representing the SAML assertion

