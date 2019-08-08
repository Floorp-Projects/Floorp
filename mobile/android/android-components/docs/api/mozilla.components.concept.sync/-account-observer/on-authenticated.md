[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [AccountObserver](index.md) / [onAuthenticated](./on-authenticated.md)

# onAuthenticated

`open fun onAuthenticated(account: `[`OAuthAccount`](../-o-auth-account/index.md)`, newAccount: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/OAuthAccount.kt#L67)

Account was successfully authenticated.

### Parameters

`account` - An authenticated instance of a [OAuthAccount](../-o-auth-account/index.md).

`newAccount` - True if this is a new account that was authenticated.