[android-components](../index.md) / [mozilla.components.feature.pwa.ext](./index.md)

## Package mozilla.components.feature.pwa.ext

### Properties

| Name | Summary |
|---|---|
| [trustedOrigins](trusted-origins.md) | `val `[`CustomTabState`](../mozilla.components.feature.customtabs.store/-custom-tab-state/index.md)`.trustedOrigins: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Nothing`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-nothing/index.html)`>`<br>Returns a list of trusted (or pending) origins. |

### Functions

| Name | Summary |
|---|---|
| [applyOrientation](apply-orientation.md) | `fun <ERROR CLASS>.applyOrientation(manifest: `[`WebAppManifest`](../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the requested orientation of the [Activity](#) to the orientation provided by the given [WebAppManifest](../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md) (See [WebAppManifest.orientation](../mozilla.components.concept.engine.manifest/-web-app-manifest/orientation.md) and [WebAppManifest.Orientation](../mozilla.components.concept.engine.manifest/-web-app-manifest/-orientation/index.md). |
| [getTrustedScope](get-trusted-scope.md) | `fun `[`WebAppManifest`](../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`.getTrustedScope(): <ERROR CLASS>?`<br>Returns the scope of the manifest as a [Uri](#) for use with [mozilla.components.feature.pwa.feature.WebAppHideToolbarFeature](../mozilla.components.feature.pwa.feature/-web-app-hide-toolbar-feature/index.md). |
| [getUrlOverride](get-url-override.md) | `fun <ERROR CLASS>.getUrlOverride(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Retrieves [String](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) Url override from the intent. |
| [getWebAppManifest](get-web-app-manifest.md) | `fun <ERROR CLASS>.getWebAppManifest(): `[`WebAppManifest`](../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`?`<br>Parses and returns the [WebAppManifest](../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md) associated with this [Bundle](#), or null if no mapping of the desired type exists. |
| [hasLargeIcons](has-large-icons.md) | `fun `[`WebAppManifest`](../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`.hasLargeIcons(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks if the web app manifest can be used to create a shortcut icon. |
| [installableManifest](installable-manifest.md) | `fun `[`Session`](../mozilla.components.browser.session/-session/index.md)`.installableManifest(): `[`WebAppManifest`](../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`?`<br>Checks if the current session represents an installable web app. If so, return the web app manifest. Otherwise, return null. |
| [putUrlOverride](put-url-override.md) | `fun <ERROR CLASS>.putUrlOverride(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): <ERROR CLASS>`<br>Add [String](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) URL override to the intent. |
| [putWebAppManifest](put-web-app-manifest.md) | `fun <ERROR CLASS>.putWebAppManifest(webAppManifest: `[`WebAppManifest`](../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Serializes and inserts a [WebAppManifest](../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md) value into the mapping of this [Bundle](#), replacing any existing web app manifest. |
| [toCustomTabConfig](to-custom-tab-config.md) | `fun `[`WebAppManifest`](../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`.toCustomTabConfig(): `[`CustomTabConfig`](../mozilla.components.browser.state.state/-custom-tab-config/index.md)<br>Creates a [CustomTabConfig](../mozilla.components.browser.state.state/-custom-tab-config/index.md) that styles a custom tab toolbar to match the manifest theme. |
| [toOrigin](to-origin.md) | `fun <ERROR CLASS>.toOrigin(): <ERROR CLASS>?`<br>Returns just the origin of the [Uri](#). |
| [toTaskDescription](to-task-description.md) | `fun `[`WebAppManifest`](../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`.toTaskDescription(icon: <ERROR CLASS>?): <ERROR CLASS>`<br>Creates a [TaskDescription](#) for the activity manager based on the manifest. |
