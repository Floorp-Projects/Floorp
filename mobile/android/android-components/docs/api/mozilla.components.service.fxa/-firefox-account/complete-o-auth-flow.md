[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FirefoxAccount](index.md) / [completeOAuthFlow](./complete-o-auth-flow.md)

# completeOAuthFlow

`fun completeOAuthFlow(code: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, state: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FirefoxAccount.kt#L133)

Overrides [OAuthAccount.completeOAuthFlow](../../mozilla.components.concept.sync/-o-auth-account/complete-o-auth-flow.md)

Authenticates the current account using the code and state parameters fetched from the
redirect URL reached after completing the sign in flow triggered by [beginOAuthFlow](begin-o-auth-flow.md).

Modifies the FirefoxAccount state.

