[android-components](../../index.md) / [mozilla.components.feature.pwa](../index.md) / [WebAppShortcutManager](./index.md)

# WebAppShortcutManager

`class WebAppShortcutManager` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/WebAppShortcutManager.kt#L57)

Helper to manage pinned shortcuts for websites.

### Parameters

`httpClient` - Fetch client used to load website icons.

`storage` - Storage used to save web app manifests to disk.

`supportWebApps` - If true, Progressive Web Apps will be pinnable.
If false, all web sites will be bookmark shortcuts even if they have a manifest.

### Types

| Name | Summary |
|---|---|
| [WebAppInstallState](-web-app-install-state/index.md) | `enum class WebAppInstallState`<br>Possible install states of a Web App. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WebAppShortcutManager(context: <ERROR CLASS>, httpClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, storage: `[`ManifestStorage`](../-manifest-storage/index.md)`, supportWebApps: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true)`<br>Helper to manage pinned shortcuts for websites. |

### Functions

| Name | Summary |
|---|---|
| [buildBasicShortcut](build-basic-shortcut.md) | `suspend fun buildBasicShortcut(context: <ERROR CLASS>, session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, overrideShortcutName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null): ShortcutInfoCompat`<br>Create a new basic pinned website shortcut using info from the session. Consuming `SHORTCUT_CATEGORY` in `AndroidManifest` is required for the package to be launched |
| [buildWebAppShortcut](build-web-app-shortcut.md) | `suspend fun buildWebAppShortcut(context: <ERROR CLASS>, manifest: `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`): ShortcutInfoCompat?`<br>Create a new Progressive Web App shortcut using a web app manifest. |
| [findShortcut](find-shortcut.md) | `fun findShortcut(context: <ERROR CLASS>, startUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Nothing`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-nothing/index.html)`?`<br>Finds the shortcut associated with the given startUrl. This method can be used to check if a web app was added to the homescreen. |
| [getWebAppInstallState](get-web-app-install-state.md) | `suspend fun getWebAppInstallState(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, currentTimeMs: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = System.currentTimeMillis()): `[`WebAppInstallState`](-web-app-install-state/index.md)<br>Checks if there is a currently installed web app to which this URL belongs. |
| [recentlyUsedWebAppsCount](recently-used-web-apps-count.md) | `suspend fun recentlyUsedWebAppsCount(activeThresholdMs: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = ManifestStorage.ACTIVE_THRESHOLD_MS): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Counts number of recently used web apps. See [ManifestStorage.activeThresholdMs](#). |
| [reportWebAppUsed](report-web-app-used.md) | `suspend fun reportWebAppUsed(manifest: `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`?`<br>Updates the usedAt timestamp of the web app this url is associated with. |
| [requestPinShortcut](request-pin-shortcut.md) | `suspend fun requestPinShortcut(context: <ERROR CLASS>, session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, overrideShortcutName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Request to create a new shortcut on the home screen. |
| [updateShortcuts](update-shortcuts.md) | `suspend fun updateShortcuts(context: <ERROR CLASS>, manifests: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Update existing PWA shortcuts with the latest info from web app manifests. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
