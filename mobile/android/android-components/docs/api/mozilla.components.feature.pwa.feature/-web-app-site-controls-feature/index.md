[android-components](../../index.md) / [mozilla.components.feature.pwa.feature](../index.md) / [WebAppSiteControlsFeature](./index.md)

# WebAppSiteControlsFeature

`class WebAppSiteControlsFeature : LifecycleObserver` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/feature/WebAppSiteControlsFeature.kt#L36)

Displays site controls notification for fullscreen web apps.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WebAppSiteControlsFeature(applicationContext: <ERROR CLASS>, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, reloadUrlUseCase: `[`ReloadUrlUseCase`](../../mozilla.components.feature.session/-session-use-cases/-reload-url-use-case/index.md)`, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, manifest: `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`?)`<br>Displays site controls notification for fullscreen web apps. |

### Functions

| Name | Summary |
|---|---|
| [onPause](on-pause.md) | `fun onPause(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onReceive](on-receive.md) | `fun onReceive(context: <ERROR CLASS>, intent: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Responds to [PendingIntent](#)s fired by the site controls notification. |
| [onResume](on-resume.md) | `fun onResume(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
