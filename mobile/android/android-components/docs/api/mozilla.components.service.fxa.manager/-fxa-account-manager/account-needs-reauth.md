[android-components](../../index.md) / [mozilla.components.service.fxa.manager](../index.md) / [FxaAccountManager](index.md) / [accountNeedsReauth](./account-needs-reauth.md)

# accountNeedsReauth

`fun accountNeedsReauth(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/manager/FxaAccountManager.kt#L377)

Indicates if account needs to be re-authenticated via [beginAuthenticationAsync](begin-authentication-async.md).
Most common reason for an account to need re-authentication is a password change.

TODO this may return a false-positive, if we're currently going through a recovery flow.
Prefer to be notified of auth problems via [AccountObserver](../../mozilla.components.concept.sync/-account-observer/index.md), which is reliable.

**Return**
A boolean flag indicating if account needs to be re-authenticated.

