---
title: FirefoxAccount.toJSONString - 
---

[mozilla.components.service.fxa](../index.html) / [FirefoxAccount](index.html) / [toJSONString](./to-j-s-o-n-string.html)

# toJSONString

`fun toJSONString(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`

Saves the current account's authentication state as a JSON string, for persistence in
the Android KeyStore/shared preferences. The authentication state can be restored using
[FirefoxAccount.fromJSONString](from-j-s-o-n-string.html).

**Return**
String containing the authentication details in JSON format

