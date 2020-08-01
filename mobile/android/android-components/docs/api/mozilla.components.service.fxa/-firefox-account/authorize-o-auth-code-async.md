[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FirefoxAccount](index.md) / [authorizeOAuthCodeAsync](./authorize-o-auth-code-async.md)

# authorizeOAuthCodeAsync

`fun authorizeOAuthCodeAsync(clientId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, scopes: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, state: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, accessType: `[`AccessType`](../../mozilla.components.concept.sync/-access-type/index.md)`): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FirefoxAccount.kt#L136)

Overrides [OAuthAccount.authorizeOAuthCodeAsync](../../mozilla.components.concept.sync/-o-auth-account/authorize-o-auth-code-async.md)

Provisions a scoped OAuth code for a given [clientId](../../mozilla.components.concept.sync/-o-auth-account/authorize-o-auth-code-async.md#mozilla.components.concept.sync.OAuthAccount$authorizeOAuthCodeAsync(kotlin.String, kotlin.Array((kotlin.String)), kotlin.String, mozilla.components.concept.sync.AccessType)/clientId) and the passed [scopes](../../mozilla.components.concept.sync/-o-auth-account/authorize-o-auth-code-async.md#mozilla.components.concept.sync.OAuthAccount$authorizeOAuthCodeAsync(kotlin.String, kotlin.Array((kotlin.String)), kotlin.String, mozilla.components.concept.sync.AccessType)/scopes).

### Parameters

`clientId` - the client id string

`scopes` - the list of scopes to request access to

`state` - the state token string

`accessType` - the accessType method to be used by the returned code, determines whether
the code can be exchanged for a refresh token to be used offline or not

**Return**
Deferred authorized auth code string, or `null` in case of failure.

