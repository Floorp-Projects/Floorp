[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [OAuthAccount](index.md) / [copyFromSessionTokenAsync](./copy-from-session-token-async.md)

# copyFromSessionTokenAsync

`abstract fun copyFromSessionTokenAsync(sessionToken: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, kSync: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, kXCS: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/OAuthAccount.kt#L174)

Attempts to migrate from an existing session token without user input.
New session token will be created.

### Parameters

`sessionToken` - token string to use for login

`kSync` - sync string for login

`kXCS` - XCS string for login

**Return**
Deferred boolean success or failure for the migration event

