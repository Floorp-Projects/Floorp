[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FirefoxAccount](index.md) / [beginPairingFlowAsync](./begin-pairing-flow-async.md)

# beginPairingFlowAsync

`fun beginPairingFlowAsync(pairingUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, scopes: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FirefoxAccount.kt#L110)

Overrides [OAuthAccount.beginPairingFlowAsync](../../mozilla.components.concept.sync/-o-auth-account/begin-pairing-flow-async.md)

Constructs a URL used to begin the pairing flow for the requested scopes and pairingUrl.

### Parameters

`pairingUrl` - URL string for pairing

`scopes` - List of OAuth scopes for which the client wants access

**Return**
Deferred AuthFlowUrl Optional that resolves to the flow URL when complete

