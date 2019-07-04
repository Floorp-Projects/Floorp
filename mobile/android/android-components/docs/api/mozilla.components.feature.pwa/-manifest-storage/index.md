[android-components](../../index.md) / [mozilla.components.feature.pwa](../index.md) / [ManifestStorage](./index.md)

# ManifestStorage

`class ManifestStorage` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/ManifestStorage.kt#L15)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ManifestStorage(context: <ERROR CLASS>)` |

### Functions

| Name | Summary |
|---|---|
| [loadManifest](load-manifest.md) | `suspend fun loadManifest(startUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`?`<br>Load a Web App Manifest for the given URL from disk. If no manifest is found, null is returned. |
| [removeManifests](remove-manifests.md) | `suspend fun removeManifests(startUrls: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): <ERROR CLASS>`<br>Delete all manifests associated with the list of URLs. |
| [saveManifest](save-manifest.md) | `suspend fun saveManifest(manifest: `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`): <ERROR CLASS>`<br>Save a Web App Manifest to disk. |
