[android-components](../../index.md) / [mozilla.components.feature.pwa.feature](../index.md) / [ManifestUpdateFeature](index.md) / [onWebAppManifestChanged](./on-web-app-manifest-changed.md)

# onWebAppManifestChanged

`fun onWebAppManifestChanged(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, manifest: `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/feature/ManifestUpdateFeature.kt#L47)

Overrides [Observer.onWebAppManifestChanged](../../mozilla.components.browser.session/-session/-observer/on-web-app-manifest-changed.md)

When the manifest is changed, compare it to the existing manifest.
If it is different, update the disk and shortcut. Ignore if called with a null
manifest or a manifest with a different start URL.

