[android-components](../index.md) / [mozilla.components.feature.pwa.ext](index.md) / [getTrustedScope](./get-trusted-scope.md)

# getTrustedScope

`fun `[`WebAppManifest`](../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`.getTrustedScope(): <ERROR CLASS>?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/ext/WebAppManifest.kt#L56)

Returns the scope of the manifest as a [Uri](#) for use
with [mozilla.components.feature.pwa.feature.WebAppHideToolbarFeature](../mozilla.components.feature.pwa.feature/-web-app-hide-toolbar-feature/index.md).

Null is returned when the scope should be ignored, such as with display: minimal-ui,
where the toolbar should always be displayed.

