[android-components](../index.md) / [mozilla.components.feature.pwa.ext](./index.md)

## Package mozilla.components.feature.pwa.ext

### Functions

| Name | Summary |
|---|---|
| [applyOrientation](apply-orientation.md) | `fun <ERROR CLASS>.applyOrientation(manifest: `[`WebAppManifest`](../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the requested orientation of the [Activity](#) to the orientation provided by the given [WebAppManifest](../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md) (See [WebAppManifest.orientation](../mozilla.components.concept.engine.manifest/-web-app-manifest/orientation.md) and [WebAppManifest.Orientation](../mozilla.components.concept.engine.manifest/-web-app-manifest/-orientation/index.md). |
| [asTaskDescription](as-task-description.md) | `fun `[`WebAppManifest`](../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`.asTaskDescription(icon: <ERROR CLASS>?): <ERROR CLASS>`<br>Create a [TaskDescription](#) for the activity manager based on the manifest. |
| [installableManifest](installable-manifest.md) | `fun `[`Session`](../mozilla.components.browser.session/-session/index.md)`.installableManifest(): `[`WebAppManifest`](../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`?`<br>Checks if the current session represents an installable web app. If so, return the web app manifest. Otherwise, return null. |
