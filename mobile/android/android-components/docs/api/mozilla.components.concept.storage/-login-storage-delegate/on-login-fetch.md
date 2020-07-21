[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [LoginStorageDelegate](index.md) / [onLoginFetch](./on-login-fetch.md)

# onLoginFetch

`abstract fun onLoginFetch(domain: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../-login/index.md)`>>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/LoginsStorage.kt#L268)

Given a [domain](on-login-fetch.md#mozilla.components.concept.storage.LoginStorageDelegate$onLoginFetch(kotlin.String)/domain), returns a [GeckoResult](#) of the matching [LoginEntry](#)s found in
[loginStorage](#).

This is called when the engine believes a field should be autofilled.

