[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [OAuthAccount](index.md) / [retryMigrateFromSessionTokenAsync](./retry-migrate-from-session-token-async.md)

# retryMigrateFromSessionTokenAsync

`abstract fun retryMigrateFromSessionTokenAsync(): Deferred<<ERROR CLASS>?>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/OAuthAccount.kt#L230)

Retries an in-flight migration attempt.

**Return**
JSON object with the result of the retry attempt or 'null' if it failed.
For up-to-date schema, see underlying implementation in https://github.com/mozilla/application-services/blob/v0.49.0/components/fxa-client/src/migrator.rs#L10
At the moment, it's just "{total_duration: long}".

