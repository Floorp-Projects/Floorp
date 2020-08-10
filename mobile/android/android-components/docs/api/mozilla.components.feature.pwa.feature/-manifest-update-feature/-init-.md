[android-components](../../index.md) / [mozilla.components.feature.pwa.feature](../index.md) / [ManifestUpdateFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`ManifestUpdateFeature(applicationContext: <ERROR CLASS>, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, shortcutManager: `[`WebAppShortcutManager`](../../mozilla.components.feature.pwa/-web-app-shortcut-manager/index.md)`, storage: `[`ManifestStorage`](../../mozilla.components.feature.pwa/-manifest-storage/index.md)`, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, initialManifest: `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`)`

Feature used to update the existing web app manifest and web app shortcut.

### Parameters

`shortcutManager` - Shortcut manager used to update pinned shortcuts.

`storage` - Manifest storage used to have updated manifests.

`sessionId` - ID of the web app session to observe.

`initialManifest` - Loaded manifest for the current web app.