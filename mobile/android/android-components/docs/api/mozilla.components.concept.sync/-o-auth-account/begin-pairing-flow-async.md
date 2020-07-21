[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [OAuthAccount](index.md) / [beginPairingFlowAsync](./begin-pairing-flow-async.md)

# beginPairingFlowAsync

`abstract fun beginPairingFlowAsync(pairingUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, scopes: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): Deferred<`[`AuthFlowUrl`](../-auth-flow-url/index.md)`?>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/OAuthAccount.kt#L87)

Constructs a URL used to begin the pairing flow for the requested scopes and pairingUrl.

### Parameters

`pairingUrl` - URL string for pairing

`scopes` - List of OAuth scopes for which the client wants access

**Return**
Deferred AuthFlowUrl Optional that resolves to the flow URL when complete

