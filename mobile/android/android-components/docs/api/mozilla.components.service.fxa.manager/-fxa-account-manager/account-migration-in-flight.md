[android-components](../../index.md) / [mozilla.components.service.fxa.manager](../index.md) / [FxaAccountManager](index.md) / [accountMigrationInFlight](./account-migration-in-flight.md)

# accountMigrationInFlight

`fun accountMigrationInFlight(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/manager/FxaAccountManager.kt#L427)

Checks if there's an in-flight account migration. An in-flight migration means that we've tried to "migrate"
via [signInWithShareableAccountAsync](sign-in-with-shareable-account-async.md) and failed for intermittent (e.g. network) reasons.
A migration sign-in attempt will be retried automatically either during account manager initialization,
or as a by-product of user-triggered [syncNowAsync](sync-now-async.md).

