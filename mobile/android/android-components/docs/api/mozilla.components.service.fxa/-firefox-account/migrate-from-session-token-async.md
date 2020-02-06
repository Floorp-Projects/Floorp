[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FirefoxAccount](index.md) / [migrateFromSessionTokenAsync](./migrate-from-session-token-async.md)

# migrateFromSessionTokenAsync

`fun migrateFromSessionTokenAsync(sessionToken: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, kSync: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, kXCS: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FirefoxAccount.kt#L159)

Overrides [OAuthAccount.migrateFromSessionTokenAsync](../../mozilla.components.concept.sync/-o-auth-account/migrate-from-session-token-async.md)

Attempts to migrate from an existing session token without user input.
Passed-in session token will be reused.

### Parameters

`sessionToken` - token string to use for login

`kSync` - sync string for login

`kXCS` - XCS string for login

**Return**
JSON object with the result of the migration or 'null' if it failed.
For up-to-date schema, see underlying implementation in https://github.com/mozilla/application-services/blob/v0.49.0/components/fxa-client/src/migrator.rs#L10
At the moment, it's just "{total_duration: long}".

