[android-components](../../index.md) / [mozilla.components.service.fxa.manager](../index.md) / [FxaAccountManager](index.md) / [authenticatedAccount](./authenticated-account.md)

# authenticatedAccount

`fun authenticatedAccount(): `[`OAuthAccount`](../../mozilla.components.concept.sync/-o-auth-account/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/manager/FxaAccountManager.kt#L359)

Main point for interaction with an [OAuthAccount](../../mozilla.components.concept.sync/-o-auth-account/index.md) instance.

**Return**
[OAuthAccount](../../mozilla.components.concept.sync/-o-auth-account/index.md) if we're in an authenticated state, null otherwise. Returned [OAuthAccount](../../mozilla.components.concept.sync/-o-auth-account/index.md)
may need to be re-authenticated; consumers are expected to check [accountNeedsReauth](account-needs-reauth.md).

