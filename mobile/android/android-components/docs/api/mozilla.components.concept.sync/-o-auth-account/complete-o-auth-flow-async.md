[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [OAuthAccount](index.md) / [completeOAuthFlowAsync](./complete-o-auth-flow-async.md)

# completeOAuthFlowAsync

`abstract fun completeOAuthFlowAsync(code: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, state: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/OAuthAccount.kt#L138)

Authenticates the current account using the [code](complete-o-auth-flow-async.md#mozilla.components.concept.sync.OAuthAccount$completeOAuthFlowAsync(kotlin.String, kotlin.String)/code) and [state](complete-o-auth-flow-async.md#mozilla.components.concept.sync.OAuthAccount$completeOAuthFlowAsync(kotlin.String, kotlin.String)/state) parameters obtained via the
OAuth flow initiated by [beginOAuthFlowAsync](begin-o-auth-flow-async.md).

Modifies the FirefoxAccount state.

### Parameters

`code` - OAuth code string

`state` - state token string

**Return**
Deferred boolean representing success or failure

