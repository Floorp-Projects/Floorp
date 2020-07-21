[android-components](../../index.md) / [mozilla.components.service.fxa.manager](../index.md) / [SignInWithShareableAccountResult](index.md) / [WillRetry](./-will-retry.md)

# WillRetry

`WillRetry` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/manager/FxaAccountManager.kt#L109)

Sign-in failed due to an intermittent problem (such as a network failure). A retry attempt will
be performed automatically during account manager initialization, or as a side-effect of certain
user actions (e.g. triggering a sync).

Applications may treat this account as "authenticated" after seeing this result.

