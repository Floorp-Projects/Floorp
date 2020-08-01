[android-components](../../index.md) / [mozilla.components.feature.pwa](../index.md) / [ManifestStorage](index.md) / [getInstalledScope](./get-installed-scope.md)

# getInstalledScope

`fun getInstalledScope(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/ManifestStorage.kt#L80)

Returns the cached scope for an url if the url falls into a web app scope that has been installed by the user.

### Parameters

`url` - the url to match against installed web app scopes.