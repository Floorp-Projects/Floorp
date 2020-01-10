[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FirefoxAccount](index.md) / [copyFromSessionTokenAsync](./copy-from-session-token-async.md)

# copyFromSessionTokenAsync

`fun copyFromSessionTokenAsync(sessionToken: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, kSync: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, kXCS: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FirefoxAccount.kt#L163)

Overrides [OAuthAccount.copyFromSessionTokenAsync](../../mozilla.components.concept.sync/-o-auth-account/copy-from-session-token-async.md)

Attempts to migrate from an existing session token without user input.
New session token will be created.

### Parameters

`sessionToken` - token string to use for login

`kSync` - sync string for login

`kXCS` - XCS string for login

**Return**
Deferred boolean success or failure for the migration event

