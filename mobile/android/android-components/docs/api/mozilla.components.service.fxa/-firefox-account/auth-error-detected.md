[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FirefoxAccount](index.md) / [authErrorDetected](./auth-error-detected.md)

# authErrorDetected

`fun authErrorDetected(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FirefoxAccount.kt#L206)

Overrides [OAuthAccount.authErrorDetected](../../mozilla.components.concept.sync/-o-auth-account/auth-error-detected.md)

Call this whenever an authentication error was encountered while using an access token
issued by [getAccessTokenAsync](../../mozilla.components.concept.sync/-o-auth-account/get-access-token-async.md).

