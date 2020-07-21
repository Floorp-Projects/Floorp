[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [AccountObserver](index.md) / [onAuthenticated](./on-authenticated.md)

# onAuthenticated

`open fun onAuthenticated(account: `[`OAuthAccount`](../-o-auth-account/index.md)`, authType: `[`AuthType`](../-auth-type/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/OAuthAccount.kt#L324)

Account was successfully authenticated.

### Parameters

`account` - An authenticated instance of a [OAuthAccount](../-o-auth-account/index.md).

`authType` - Describes what kind of authentication event caused this invocation.