[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [OAuthAccount](index.md) / [isInMigrationState](./is-in-migration-state.md)

# isInMigrationState

`abstract fun isInMigrationState(): `[`InFlightMigrationState`](../-in-flight-migration-state/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/OAuthAccount.kt#L222)

Checks if there's a migration in-flight. An in-flight migration means that we've tried to migrate
via either [migrateFromSessionTokenAsync](migrate-from-session-token-async.md) or [copyFromSessionTokenAsync](copy-from-session-token-async.md), and failed for intermittent
(e.g. network)
reasons. When an in-flight migration is present, we can retry using [retryMigrateFromSessionTokenAsync](retry-migrate-from-session-token-async.md).

**Return**
InFlightMigrationState indicating specific migration state.

