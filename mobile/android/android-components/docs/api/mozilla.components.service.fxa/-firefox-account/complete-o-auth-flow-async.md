[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FirefoxAccount](index.md) / [completeOAuthFlowAsync](./complete-o-auth-flow-async.md)

# completeOAuthFlowAsync

`fun completeOAuthFlowAsync(code: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, state: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FirefoxAccount.kt#L199)

Overrides [OAuthAccount.completeOAuthFlowAsync](../../mozilla.components.concept.sync/-o-auth-account/complete-o-auth-flow-async.md)

Authenticates the current account using the [code](../../mozilla.components.concept.sync/-o-auth-account/complete-o-auth-flow-async.md#mozilla.components.concept.sync.OAuthAccount$completeOAuthFlowAsync(kotlin.String, kotlin.String)/code) and [state](../../mozilla.components.concept.sync/-o-auth-account/complete-o-auth-flow-async.md#mozilla.components.concept.sync.OAuthAccount$completeOAuthFlowAsync(kotlin.String, kotlin.String)/state) parameters obtained via the
OAuth flow initiated by [beginOAuthFlowAsync](../../mozilla.components.concept.sync/-o-auth-account/begin-o-auth-flow-async.md).

Modifies the FirefoxAccount state.

### Parameters

`code` - OAuth code string

`state` - state token string

**Return**
Deferred boolean representing success or failure

