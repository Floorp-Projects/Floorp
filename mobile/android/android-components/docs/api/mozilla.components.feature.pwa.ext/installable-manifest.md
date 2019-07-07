[android-components](../index.md) / [mozilla.components.feature.pwa.ext](index.md) / [installableManifest](./installable-manifest.md)

# installableManifest

`fun `[`Session`](../mozilla.components.browser.session/-session/index.md)`.installableManifest(): `[`WebAppManifest`](../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/ext/Session.kt#L22)

Checks if the current session represents an installable web app.
If so, return the web app manifest. Otherwise, return null.

Websites are installable if:

* The site is served over HTTPS
* The site has a valid manifest with a name or short_name
* The icons array in the manifest contains an icon of at least 192x192
