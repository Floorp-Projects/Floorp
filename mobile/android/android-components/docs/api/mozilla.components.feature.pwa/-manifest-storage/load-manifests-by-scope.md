[android-components](../../index.md) / [mozilla.components.feature.pwa](../index.md) / [ManifestStorage](index.md) / [loadManifestsByScope](./load-manifests-by-scope.md)

# loadManifestsByScope

`suspend fun loadManifestsByScope(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/ManifestStorage.kt#L45)

Load all Web App Manifests with a matching scope for the given URL from disk.
If no manifests are found, an empty list is returned.

### Parameters

`url` - URL of site. Should correspond to an url covered by the scope of a stored manifest.