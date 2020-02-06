[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [OAuthAccount](index.md) / [beginOAuthFlowAsync](./begin-o-auth-flow-async.md)

# beginOAuthFlowAsync

`abstract fun beginOAuthFlowAsync(scopes: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): Deferred<`[`AuthFlowUrl`](../-auth-flow-url/index.md)`?>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/OAuthAccount.kt#L78)

Constructs a URL used to begin the OAuth flow for the requested scopes and keys.

### Parameters

`scopes` - List of OAuth scopes for which the client wants access

**Return**
Deferred AuthFlowUrl that resolves to the flow URL when complete

