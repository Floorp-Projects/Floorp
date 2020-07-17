[android-components](../../index.md) / [mozilla.components.feature.pwa.feature](../index.md) / [ManifestUpdateFeature](./index.md)

# ManifestUpdateFeature

`class ManifestUpdateFeature : `[`Observer`](../../mozilla.components.browser.session/-session/-observer/index.md)`, `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/feature/ManifestUpdateFeature.kt#L29)

Feature used to update the existing web app manifest and web app shortcut.

### Parameters

`shortcutManager` - Shortcut manager used to update pinned shortcuts.

`storage` - Manifest storage used to have updated manifests.

`sessionId` - ID of the web app session to observe.

`initialManifest` - Loaded manifest for the current web app.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ManifestUpdateFeature(applicationContext: <ERROR CLASS>, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, shortcutManager: `[`WebAppShortcutManager`](../../mozilla.components.feature.pwa/-web-app-shortcut-manager/index.md)`, storage: `[`ManifestStorage`](../../mozilla.components.feature.pwa/-manifest-storage/index.md)`, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, initialManifest: `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`)`<br>Feature used to update the existing web app manifest and web app shortcut. |

### Functions

| Name | Summary |
|---|---|
| [onWebAppManifestChanged](on-web-app-manifest-changed.md) | `fun onWebAppManifestChanged(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, manifest: `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>When the manifest is changed, compare it to the existing manifest. If it is different, update the disk and shortcut. Ignore if called with a null manifest or a manifest with a different start URL. |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inherited Functions

| Name | Summary |
|---|---|
| [onAppPermissionRequested](../../mozilla.components.browser.session/-session/-observer/on-app-permission-requested.md) | `open fun onAppPermissionRequested(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, permissionRequest: `[`PermissionRequest`](../../mozilla.components.concept.engine.permission/-permission-request/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onContentPermissionRequested](../../mozilla.components.browser.session/-session/-observer/on-content-permission-requested.md) | `open fun onContentPermissionRequested(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, permissionRequest: `[`PermissionRequest`](../../mozilla.components.concept.engine.permission/-permission-request/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onCrashStateChanged](../../mozilla.components.browser.session/-session/-observer/on-crash-state-changed.md) | `open fun onCrashStateChanged(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, crashed: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onCustomTabConfigChanged](../../mozilla.components.browser.session/-session/-observer/on-custom-tab-config-changed.md) | `open fun onCustomTabConfigChanged(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, customTabConfig: `[`CustomTabConfig`](../../mozilla.components.browser.state.state/-custom-tab-config/index.md)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onDesktopModeChanged](../../mozilla.components.browser.session/-session/-observer/on-desktop-mode-changed.md) | `open fun onDesktopModeChanged(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, enabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onLaunchIntentRequest](../../mozilla.components.browser.session/-session/-observer/on-launch-intent-request.md) | `open fun onLaunchIntentRequest(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, appIntent: <ERROR CLASS>?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onLoadRequest](../../mozilla.components.browser.session/-session/-observer/on-load-request.md) | `open fun onLoadRequest(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, triggeredByRedirect: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, triggeredByWebContent: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onLoadingStateChanged](../../mozilla.components.browser.session/-session/-observer/on-loading-state-changed.md) | `open fun onLoadingStateChanged(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, loading: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onNavigationStateChanged](../../mozilla.components.browser.session/-session/-observer/on-navigation-state-changed.md) | `open fun onNavigationStateChanged(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, canGoBack: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, canGoForward: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onProgress](../../mozilla.components.browser.session/-session/-observer/on-progress.md) | `open fun onProgress(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, progress: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onRecordingDevicesChanged](../../mozilla.components.browser.session/-session/-observer/on-recording-devices-changed.md) | `open fun onRecordingDevicesChanged(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, devices: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`RecordingDevice`](../../mozilla.components.concept.engine.media/-recording-device/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onSearch](../../mozilla.components.browser.session/-session/-observer/on-search.md) | `open fun onSearch(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onSecurityChanged](../../mozilla.components.browser.session/-session/-observer/on-security-changed.md) | `open fun onSecurityChanged(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, securityInfo: `[`SecurityInfo`](../../mozilla.components.browser.session/-session/-security-info/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onThumbnailChanged](../../mozilla.components.browser.session/-session/-observer/on-thumbnail-changed.md) | `open fun onThumbnailChanged(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, bitmap: <ERROR CLASS>?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onTitleChanged](../../mozilla.components.browser.session/-session/-observer/on-title-changed.md) | `open fun onTitleChanged(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onTrackerBlocked](../../mozilla.components.browser.session/-session/-observer/on-tracker-blocked.md) | `open fun onTrackerBlocked(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, tracker: `[`Tracker`](../../mozilla.components.concept.engine.content.blocking/-tracker/index.md)`, all: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Tracker`](../../mozilla.components.concept.engine.content.blocking/-tracker/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onTrackerBlockingEnabledChanged](../../mozilla.components.browser.session/-session/-observer/on-tracker-blocking-enabled-changed.md) | `open fun onTrackerBlockingEnabledChanged(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, blockingEnabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onTrackerLoaded](../../mozilla.components.browser.session/-session/-observer/on-tracker-loaded.md) | `open fun onTrackerLoaded(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, tracker: `[`Tracker`](../../mozilla.components.concept.engine.content.blocking/-tracker/index.md)`, all: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Tracker`](../../mozilla.components.concept.engine.content.blocking/-tracker/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onUrlChanged](../../mozilla.components.browser.session/-session/-observer/on-url-changed.md) | `open fun onUrlChanged(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
