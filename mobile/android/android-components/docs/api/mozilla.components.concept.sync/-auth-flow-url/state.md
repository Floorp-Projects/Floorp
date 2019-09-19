[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [AuthFlowUrl](index.md) / [state](./state.md)

# state

`val state: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/OAuthAccount.kt#L29)

OAuth state parameter, identifying a specific authentication flow.
This string is randomly generated during [OAuthAccount.beginOAuthFlowAsync](../-o-auth-account/begin-o-auth-flow-async.md) and [OAuthAccount.beginPairingFlowAsync](../-o-auth-account/begin-pairing-flow-async.md).

### Property

`state` - OAuth state parameter, identifying a specific authentication flow.
This string is randomly generated during [OAuthAccount.beginOAuthFlowAsync](../-o-auth-account/begin-o-auth-flow-async.md) and [OAuthAccount.beginPairingFlowAsync](../-o-auth-account/begin-pairing-flow-async.md).