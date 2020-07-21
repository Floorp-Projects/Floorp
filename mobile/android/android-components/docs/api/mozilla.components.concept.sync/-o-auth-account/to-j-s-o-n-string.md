[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [OAuthAccount](index.md) / [toJSONString](./to-j-s-o-n-string.md)

# toJSONString

`abstract fun toJSONString(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/OAuthAccount.kt#L257)

Serializes the current account's authentication state as a JSON string, for persistence in
the Android KeyStore/shared preferences. The authentication state can be restored using
[FirefoxAccount.fromJSONString](#).

**Return**
String containing the authentication details in JSON format

