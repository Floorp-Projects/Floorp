[android-components](../../index.md) / [mozilla.components.feature.pwa](../index.md) / [WebAppShortcutManager](index.md) / [updateShortcuts](./update-shortcuts.md)

# updateShortcuts

`suspend fun updateShortcuts(context: <ERROR CLASS>, manifests: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/WebAppShortcutManager.kt#L92)

Update existing PWA shortcuts with the latest info from web app manifests.

Devices before 7.1 do not allow shortcuts to be dynamically updated,
so this method will do nothing.

