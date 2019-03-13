[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FirefoxAccount](index.md) / [registerPersistCallback](./register-persist-callback.md)

# registerPersistCallback

`fun registerPersistCallback(persistCallback: `[`PersistCallback`](../-persist-callback.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FirefoxAccount.kt#L62)

Registers a [PersistCallback](../-persist-callback.md) that will be called every time the
[FirefoxAccount](index.md) internal state has mutated.
The [FirefoxAccount](index.md) instance can be later restored using the
[FirefoxAccount.fromJSONString](from-j-s-o-n-string.md) class method.
It is the responsibility of the consumer to ensure the persisted data
is saved in a secure location, as it can contain Sync Keys and
OAuth tokens.

