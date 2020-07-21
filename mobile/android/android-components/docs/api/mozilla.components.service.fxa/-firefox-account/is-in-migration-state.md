[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FirefoxAccount](index.md) / [isInMigrationState](./is-in-migration-state.md)

# isInMigrationState

`fun isInMigrationState(): `[`InFlightMigrationState`](../../mozilla.components.concept.sync/-in-flight-migration-state/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FirefoxAccount.kt#L171)

Overrides [OAuthAccount.isInMigrationState](../../mozilla.components.concept.sync/-o-auth-account/is-in-migration-state.md)

Checks if there's a migration in-flight. An in-flight migration means that we've tried to migrate
via either [migrateFromSessionTokenAsync](../../mozilla.components.concept.sync/-o-auth-account/migrate-from-session-token-async.md) or [copyFromSessionTokenAsync](../../mozilla.components.concept.sync/-o-auth-account/copy-from-session-token-async.md), and failed for intermittent
(e.g. network)
reasons. When an in-flight migration is present, we can retry using [retryMigrateFromSessionTokenAsync](../../mozilla.components.concept.sync/-o-auth-account/retry-migrate-from-session-token-async.md).

**Return**
InFlightMigrationState indicating specific migration state.

