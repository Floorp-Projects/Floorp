[android-components](../../index.md) / [mozilla.components.feature.pwa](../index.md) / [ManifestStorage](./index.md)

# ManifestStorage

`class ManifestStorage` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/ManifestStorage.kt#L23)

Disk storage for [WebAppManifest](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md). Other components use this class to reload a saved manifest.

### Parameters

`context` - the application context this storage is associated with

`activeThresholdMs` - a timeout in milliseconds after which the storage will consider a manifest
    as unused. By default this is [ACTIVE_THRESHOLD_MS](-a-c-t-i-v-e_-t-h-r-e-s-h-o-l-d_-m-s.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ManifestStorage(context: <ERROR CLASS>, activeThresholdMs: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = ACTIVE_THRESHOLD_MS)`<br>Disk storage for [WebAppManifest](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md). Other components use this class to reload a saved manifest. |

### Functions

| Name | Summary |
|---|---|
| [getInstalledScope](get-installed-scope.md) | `fun getInstalledScope(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Returns the cached scope for an url if the url falls into a web app scope that has been installed by the user. |
| [getStartUrlForInstalledScope](get-start-url-for-installed-scope.md) | `fun getStartUrlForInstalledScope(scope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Returns a cached start url for an installed web app scope. |
| [hasRecentManifest](has-recent-manifest.md) | `suspend fun hasRecentManifest(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, currentTimeMs: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = System.currentTimeMillis()): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks whether there is a currently used manifest with a scope that matches the url. |
| [loadManifest](load-manifest.md) | `suspend fun loadManifest(startUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`?`<br>Load a Web App Manifest for the given URL from disk. If no manifest is found, null is returned. |
| [loadManifestsByScope](load-manifests-by-scope.md) | `suspend fun loadManifestsByScope(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`>`<br>Load all Web App Manifests with a matching scope for the given URL from disk. If no manifests are found, an empty list is returned. |
| [loadShareableManifests](load-shareable-manifests.md) | `suspend fun loadShareableManifests(currentTime: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`>`<br>Load all Web App Manifests that contain share targets. If no manifests are found, an empty list is returned. |
| [recentManifestsCount](recent-manifests-count.md) | `suspend fun recentManifestsCount(activeThresholdMs: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = this.activeThresholdMs, currentTimeMs: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = System.currentTimeMillis()): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Counts number of recently used manifests, as configured by [activeThresholdMs](recent-manifests-count.md#mozilla.components.feature.pwa.ManifestStorage$recentManifestsCount(kotlin.Long, kotlin.Long)/activeThresholdMs). |
| [removeManifests](remove-manifests.md) | `suspend fun removeManifests(startUrls: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): <ERROR CLASS>`<br>Delete all manifests associated with the list of URLs. |
| [saveManifest](save-manifest.md) | `suspend fun saveManifest(manifest: `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`): <ERROR CLASS>`<br>Save a Web App Manifest to disk. |
| [updateManifest](update-manifest.md) | `suspend fun updateManifest(manifest: `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`): <ERROR CLASS>`<br>Update an existing Web App Manifest on disk. |
| [updateManifestUsedAt](update-manifest-used-at.md) | `suspend fun updateManifestUsedAt(manifest: `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`): <ERROR CLASS>`<br>Update the last time a web app was used. |
| [warmUpScopes](warm-up-scopes.md) | `suspend fun warmUpScopes(currentTime: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): <ERROR CLASS>`<br>Populates a cache of currently installed web app scopes and their start urls. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [ACTIVE_THRESHOLD_MS](-a-c-t-i-v-e_-t-h-r-e-s-h-o-l-d_-m-s.md) | `const val ACTIVE_THRESHOLD_MS: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
