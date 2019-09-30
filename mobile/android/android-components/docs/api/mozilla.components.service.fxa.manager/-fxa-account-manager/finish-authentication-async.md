[android-components](../../index.md) / [mozilla.components.service.fxa.manager](../index.md) / [FxaAccountManager](index.md) / [finishAuthenticationAsync](./finish-authentication-async.md)

# finishAuthenticationAsync

`fun finishAuthenticationAsync(authData: `[`FxaAuthData`](../../mozilla.components.service.fxa/-fxa-auth-data/index.md)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/manager/FxaAccountManager.kt#L473)

Finalize authentication that was started via [beginAuthenticationAsync](begin-authentication-async.md).

If authentication wasn't started via this manager we won't accept this authentication attempt,
returning `false`. This may happen if [WebChannelFeature](#) is enabled, and user is manually
logging into accounts.firefox.com in a regular tab.

Guiding principle behind this is that logging into accounts.firefox.com should not affect
logged-in state of the browser itself, even though the two may have an established communication
channel via [WebChannelFeature](#).

**Return**
A deferred boolean flag indicating if authentication state was accepted.

