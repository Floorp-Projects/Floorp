[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FirefoxAccount](index.md) / [completeOAuthFlowAsync](./complete-o-auth-flow-async.md)

# completeOAuthFlowAsync

`fun completeOAuthFlowAsync(code: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, state: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FirefoxAccount.kt#L174)

Overrides [OAuthAccount.completeOAuthFlowAsync](../../mozilla.components.concept.sync/-o-auth-account/complete-o-auth-flow-async.md)

Authenticates the current account using the code and state parameters fetched from the
redirect URL reached after completing the sign in flow triggered by [beginOAuthFlowAsync](begin-o-auth-flow-async.md).

Modifies the FirefoxAccount state.

