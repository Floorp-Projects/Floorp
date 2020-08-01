[android-components](../index.md) / [mozilla.components.service.fxa](index.md) / [asSyncAuthInfo](./as-sync-auth-info.md)

# asSyncAuthInfo

`fun `[`AccessTokenInfo`](../mozilla.components.concept.sync/-access-token-info/index.md)`.asSyncAuthInfo(tokenServerUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`SyncAuthInfo`](../mozilla.components.concept.sync/-sync-auth-info/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/Types.kt#L75)

Converts a generic [AccessTokenInfo](#) into a Firefox Sync-friendly [SyncAuthInfo](../mozilla.components.concept.sync/-sync-auth-info/index.md) instance which
may be used for data synchronization.

### Exceptions

`AuthException` - if [AccessTokenInfo](#) didn't have key information.

**Return**
An [SyncAuthInfo](../mozilla.components.concept.sync/-sync-auth-info/index.md) which is guaranteed to have a sync key.

