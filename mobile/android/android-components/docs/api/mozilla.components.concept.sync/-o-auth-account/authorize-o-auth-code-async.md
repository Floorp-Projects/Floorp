[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [OAuthAccount](index.md) / [authorizeOAuthCodeAsync](./authorize-o-auth-code-async.md)

# authorizeOAuthCodeAsync

`abstract fun authorizeOAuthCodeAsync(clientId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, scopes: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, state: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, accessType: `[`AccessType`](../-access-type/index.md)` = AccessType.ONLINE): Deferred<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/OAuthAccount.kt#L113)

Provisions a scoped OAuth code for a given [clientId](authorize-o-auth-code-async.md#mozilla.components.concept.sync.OAuthAccount$authorizeOAuthCodeAsync(kotlin.String, kotlin.Array((kotlin.String)), kotlin.String, mozilla.components.concept.sync.AccessType)/clientId) and the passed [scopes](authorize-o-auth-code-async.md#mozilla.components.concept.sync.OAuthAccount$authorizeOAuthCodeAsync(kotlin.String, kotlin.Array((kotlin.String)), kotlin.String, mozilla.components.concept.sync.AccessType)/scopes).

### Parameters

`clientId` - the client id string

`scopes` - the list of scopes to request access to

`state` - the state token string

`accessType` - the accessType method to be used by the returned code, determines whether
the code can be exchanged for a refresh token to be used offline or not

**Return**
Deferred authorized auth code string, or `null` in case of failure.

