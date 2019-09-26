[android-components](../../index.md) / [mozilla.components.feature.pwa](../index.md) / [WebAppShortcutManager](./index.md)

# WebAppShortcutManager

`class WebAppShortcutManager` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/WebAppShortcutManager.kt#L47)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WebAppShortcutManager(context: <ERROR CLASS>, httpClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, storage: `[`ManifestStorage`](../-manifest-storage/index.md)` = ManifestStorage(context), supportWebApps: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true)` |

### Functions

| Name | Summary |
|---|---|
| [buildBasicShortcut](build-basic-shortcut.md) | `fun buildBasicShortcut(context: <ERROR CLASS>, session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, overrideShortcutName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null): ShortcutInfoCompat`<br>Create a new basic pinned website shortcut using info from the session. |
| [buildWebAppShortcut](build-web-app-shortcut.md) | `suspend fun buildWebAppShortcut(context: <ERROR CLASS>, manifest: `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`): ShortcutInfoCompat?`<br>Create a new Progressive Web App shortcut using a web app manifest. |
| [findShortcut](find-shortcut.md) | `fun findShortcut(context: <ERROR CLASS>, startUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Nothing`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-nothing/index.html)`?`<br>Finds the shortcut associated with the given startUrl. This method can be used to check if a web app was added to the homescreen. |
| [requestPinShortcut](request-pin-shortcut.md) | `suspend fun requestPinShortcut(context: <ERROR CLASS>, session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, overrideShortcutName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Request to create a new shortcut on the home screen. |
| [uninstallShortcuts](uninstall-shortcuts.md) | `suspend fun uninstallShortcuts(context: <ERROR CLASS>, startUrls: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, disabledMessage: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Uninstalls a set of PWAs from the user's device by disabling their shortcuts and removing the associated manifest data. |
| [updateShortcuts](update-shortcuts.md) | `suspend fun updateShortcuts(context: <ERROR CLASS>, manifests: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Update existing PWA shortcuts with the latest info from web app manifests. |
