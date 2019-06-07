[android-components](../../index.md) / [mozilla.components.service.fxa.manager](../index.md) / [FxaAccountManager](index.md) / [accountNeedsReauth](./account-needs-reauth.md)

# accountNeedsReauth

`fun accountNeedsReauth(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/manager/FxaAccountManager.kt#L245)

Indicates if account needs to be re-authenticated via [beginAuthenticationAsync](begin-authentication-async.md).
Most common reason for an account to need re-authentication is a password change.

**Return**
A boolean flag indicating if account needs to be re-authenticated.

