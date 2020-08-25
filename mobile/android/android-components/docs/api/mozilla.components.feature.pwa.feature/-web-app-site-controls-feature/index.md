[android-components](../../index.md) / [mozilla.components.feature.pwa.feature](../index.md) / [WebAppSiteControlsFeature](./index.md)

# WebAppSiteControlsFeature

`class WebAppSiteControlsFeature : LifecycleObserver` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/feature/WebAppSiteControlsFeature.kt#L43)

Displays site controls notification for fullscreen web apps.

### Parameters

`sessionId` - ID of the web app session to observe.

`manifest` - Web App Manifest reference used to populate the notification.

`controlsBuilder` - Customizes the created notification.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WebAppSiteControlsFeature(applicationContext: <ERROR CLASS>, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, reloadUrlUseCase: `[`ReloadUrlUseCase`](../../mozilla.components.feature.session/-session-use-cases/-reload-url-use-case/index.md)`, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, manifest: `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`? = null, controlsBuilder: `[`SiteControlsBuilder`](../-site-controls-builder/index.md)` = SiteControlsBuilder.CopyAndRefresh(reloadUrlUseCase), icons: `[`BrowserIcons`](../../mozilla.components.browser.icons/-browser-icons/index.md)`? = null)``WebAppSiteControlsFeature(applicationContext: <ERROR CLASS>, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, manifest: `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`? = null, controlsBuilder: `[`SiteControlsBuilder`](../-site-controls-builder/index.md)` = SiteControlsBuilder.Default(), icons: `[`BrowserIcons`](../../mozilla.components.browser.icons/-browser-icons/index.md)`? = null)`<br>Displays site controls notification for fullscreen web apps. |

### Functions

| Name | Summary |
|---|---|
| [onCreate](on-create.md) | `fun onCreate(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts loading the [notificationIcon](#) on create. |
| [onDestroy](on-destroy.md) | `fun onDestroy(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Cancels the [notificationIcon](#) loading job on destroy. |
| [onPause](on-pause.md) | `fun onPause(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Cancels the site controls notification and unregisters the broadcast receiver. |
| [onReceive](on-receive.md) | `fun onReceive(context: <ERROR CLASS>, intent: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Responds to [PendingIntent](#)s fired by the site controls notification. |
| [onResume](on-resume.md) | `fun onResume(owner: LifecycleOwner): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Displays a notification from the given [SiteControlsBuilder.buildNotification](../-site-controls-builder/build-notification.md) that will be shown as long as the lifecycle is in the foreground. Registers this class as a broadcast receiver to receive events from the notification and call [SiteControlsBuilder.onReceiveBroadcast](../-site-controls-builder/on-receive-broadcast.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
