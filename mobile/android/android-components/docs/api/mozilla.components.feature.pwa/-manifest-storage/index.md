[android-components](../../index.md) / [mozilla.components.feature.pwa](../index.md) / [ManifestStorage](./index.md)

# ManifestStorage

`class ManifestStorage` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/ManifestStorage.kt#L23)

Disk storage for [WebAppManifest](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md). Other components use this class to reload a saved manifest.

### Parameters

`context` - the application context this storage is associated with

`usedAtTimeout` - a timeout in milliseconds after which the storage will consider a manifest
    as unused. By default this is 30 days.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ManifestStorage(context: <ERROR CLASS>, usedAtTimeout: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = DEFAULT_TIMEOUT)`<br>Disk storage for [WebAppManifest](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md). Other components use this class to reload a saved manifest. |

### Functions

| Name | Summary |
|---|---|
| [hasRecentManifest](has-recent-manifest.md) | `suspend fun hasRecentManifest(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, currentTime: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks whether there is a currently used manifest with a scope that matches the url. |
| [loadManifest](load-manifest.md) | `suspend fun loadManifest(startUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`?`<br>Load a Web App Manifest for the given URL from disk. If no manifest is found, null is returned. |
| [loadManifestsByScope](load-manifests-by-scope.md) | `suspend fun loadManifestsByScope(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`>`<br>Load all Web App Manifests with a matching scope for the given URL from disk. If no manifest is found, an empty list is returned. |
| [removeManifests](remove-manifests.md) | `suspend fun removeManifests(startUrls: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): <ERROR CLASS>`<br>Delete all manifests associated with the list of URLs. |
| [saveManifest](save-manifest.md) | `suspend fun saveManifest(manifest: `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`): <ERROR CLASS>`<br>Save a Web App Manifest to disk. |
| [updateManifest](update-manifest.md) | `suspend fun updateManifest(manifest: `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`): <ERROR CLASS>`<br>Update an existing Web App Manifest on disk. |
| [updateManifestUsedAt](update-manifest-used-at.md) | `suspend fun updateManifestUsedAt(manifest: `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`): <ERROR CLASS>`<br>Update the last time a web app was used. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [DEFAULT_TIMEOUT](-d-e-f-a-u-l-t_-t-i-m-e-o-u-t.md) | `const val DEFAULT_TIMEOUT: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |
| [MILLISECONDS_IN_A_DAY](-m-i-l-l-i-s-e-c-o-n-d-s_-i-n_-a_-d-a-y.md) | `const val MILLISECONDS_IN_A_DAY: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
