[android-components](../../index.md) / [mozilla.components.feature.pwa](../index.md) / [WebAppShortcutManager](index.md) / [reportWebAppUsed](./report-web-app-used.md)

# reportWebAppUsed

`suspend fun reportWebAppUsed(manifest: `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/WebAppShortcutManager.kt#L235)

Updates the usedAt timestamp of the web app this url is associated with.

### Parameters

`manifest` - the manifest to update