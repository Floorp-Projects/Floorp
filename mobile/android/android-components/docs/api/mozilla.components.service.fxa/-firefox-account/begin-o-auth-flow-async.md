[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FirefoxAccount](index.md) / [beginOAuthFlowAsync](./begin-o-auth-flow-async.md)

# beginOAuthFlowAsync

`fun beginOAuthFlowAsync(scopes: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, wantsKeys: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): Deferred<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FirefoxAccount.kt#L110)

Overrides [OAuthAccount.beginOAuthFlowAsync](../../mozilla.components.concept.sync/-o-auth-account/begin-o-auth-flow-async.md)

Constructs a URL used to begin the OAuth flow for the requested scopes and keys.

### Parameters

`scopes` - List of OAuth scopes for which the client wants access

`wantsKeys` - Fetch keys for end-to-end encryption of data from Mozilla-hosted services

**Return**
Deferred that resolves to the flow URL when complete

