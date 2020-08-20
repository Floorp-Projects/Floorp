[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FirefoxAccount](index.md) / [retryMigrateFromSessionTokenAsync](./retry-migrate-from-session-token-async.md)

# retryMigrateFromSessionTokenAsync

`fun retryMigrateFromSessionTokenAsync(): Deferred<<ERROR CLASS>?>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FirefoxAccount.kt#L178)

Overrides [OAuthAccount.retryMigrateFromSessionTokenAsync](../../mozilla.components.concept.sync/-o-auth-account/retry-migrate-from-session-token-async.md)

Retries an in-flight migration attempt.

**Return**
JSON object with the result of the retry attempt or 'null' if it failed.
For up-to-date schema, see underlying implementation in https://github.com/mozilla/application-services/blob/v0.49.0/components/fxa-client/src/migrator.rs#L10
At the moment, it's just "{total_duration: long}".

