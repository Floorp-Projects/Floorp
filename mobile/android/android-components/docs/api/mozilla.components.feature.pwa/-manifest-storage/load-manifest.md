[android-components](../../index.md) / [mozilla.components.feature.pwa](../index.md) / [ManifestStorage](index.md) / [loadManifest](./load-manifest.md)

# loadManifest

`suspend fun loadManifest(startUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/ManifestStorage.kt#L26)

Load a Web App Manifest for the given URL from disk.
If no manifest is found, null is returned.

### Parameters

`startUrl` - URL of site. Should correspond to manifest's start_url.