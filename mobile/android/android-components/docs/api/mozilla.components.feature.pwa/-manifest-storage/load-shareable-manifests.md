[android-components](../../index.md) / [mozilla.components.feature.pwa](../index.md) / [ManifestStorage](index.md) / [loadShareableManifests](./load-shareable-manifests.md)

# loadShareableManifests

`suspend fun loadShareableManifests(currentTime: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/ManifestStorage.kt#L108)

Load all Web App Manifests that contain share targets.
If no manifests are found, an empty list is returned.

### Parameters

`currentTime` - the current time in milliseconds.