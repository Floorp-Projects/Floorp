[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FirefoxAccount](index.md) / [fromJSONString](./from-j-s-o-n-string.md)

# fromJSONString

`fun fromJSONString(json: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, persistCallback: `[`PersistCallback`](../-persist-callback.md)`? = null): `[`FirefoxAccount`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FirefoxAccount.kt#L288)

Restores the account's authentication state from a JSON string produced by
[FirefoxAccount.toJSONString](to-j-s-o-n-string.md).

### Parameters

`persistCallback` - This callback will be called every time the [FirefoxAccount](index.md)
internal state has mutated.
The FirefoxAccount instance can be later restored using the
[FirefoxAccount.fromJSONString](./from-j-s-o-n-string.md)` class method.
It is the responsibility of the consumer to ensure the persisted data
is saved in a secure location, as it can contain Sync Keys and
OAuth tokens.

**Return**
[FirefoxAccount](index.md) representing the authentication state

