[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FirefoxAccount](index.md) / [checkAuthorizationStatusAsync](./check-authorization-status-async.md)

# checkAuthorizationStatusAsync

`fun checkAuthorizationStatusAsync(singleScope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`?>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FirefoxAccount.kt#L210)

Overrides [OAuthAccount.checkAuthorizationStatusAsync](../../mozilla.components.concept.sync/-o-auth-account/check-authorization-status-async.md)

This method should be called when a request made with an OAuth token failed with an
authentication error. It will re-build cached state and perform a connectivity check.

This will use the network.

In time, fxalib will grow a similar method, at which point we'll just relay to it.
See https://github.com/mozilla/application-services/issues/1263

### Parameters

`singleScope` - An oauth scope for which to check authorization state.

**Return**
An optional [Boolean](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) flag indicating if we're connected, or need to go through
re-authentication. A null result means we were not able to determine state at this time.

