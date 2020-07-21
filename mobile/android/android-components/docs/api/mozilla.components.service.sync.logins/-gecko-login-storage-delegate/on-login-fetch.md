[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [GeckoLoginStorageDelegate](index.md) / [onLoginFetch](./on-login-fetch.md)

# onLoginFetch

`fun onLoginFetch(domain: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../../mozilla.components.concept.storage/-login/index.md)`>>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/GeckoLoginStorageDelegate.kt#L61)

Overrides [LoginStorageDelegate.onLoginFetch](../../mozilla.components.concept.storage/-login-storage-delegate/on-login-fetch.md)

Given a [domain](../../mozilla.components.concept.storage/-login-storage-delegate/on-login-fetch.md#mozilla.components.concept.storage.LoginStorageDelegate$onLoginFetch(kotlin.String)/domain), returns a [GeckoResult](#) of the matching [LoginEntry](#)s found in
[loginStorage](#).

This is called when the engine believes a field should be autofilled.

