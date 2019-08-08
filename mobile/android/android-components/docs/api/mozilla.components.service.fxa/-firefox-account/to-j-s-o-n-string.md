[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FirefoxAccount](index.md) / [toJSONString](./to-j-s-o-n-string.md)

# toJSONString

`fun toJSONString(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FirefoxAccount.kt#L271)

Overrides [OAuthAccount.toJSONString](../../mozilla.components.concept.sync/-o-auth-account/to-j-s-o-n-string.md)

Saves the current account's authentication state as a JSON string, for persistence in
the Android KeyStore/shared preferences. The authentication state can be restored using
[FirefoxAccount.fromJSONString](from-j-s-o-n-string.md).

**Return**
String containing the authentication details in JSON format

