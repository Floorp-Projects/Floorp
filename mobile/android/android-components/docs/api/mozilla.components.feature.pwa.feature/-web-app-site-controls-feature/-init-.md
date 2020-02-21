[android-components](../../index.md) / [mozilla.components.feature.pwa.feature](../index.md) / [WebAppSiteControlsFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`WebAppSiteControlsFeature(applicationContext: <ERROR CLASS>, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, reloadUrlUseCase: `[`ReloadUrlUseCase`](../../mozilla.components.feature.session/-session-use-cases/-reload-url-use-case/index.md)`, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, manifest: `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`? = null, controlsBuilder: `[`SiteControlsBuilder`](../-site-controls-builder/index.md)` = SiteControlsBuilder.CopyAndRefresh(reloadUrlUseCase))``WebAppSiteControlsFeature(applicationContext: <ERROR CLASS>, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, manifest: `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`? = null, controlsBuilder: `[`SiteControlsBuilder`](../-site-controls-builder/index.md)` = SiteControlsBuilder.Default())`

Displays site controls notification for fullscreen web apps.

### Parameters

`sessionId` - ID of the web app session to observe.

`manifest` - Web App Manifest reference used to populate the notification.

`controlsBuilder` - Customizes the created notification.