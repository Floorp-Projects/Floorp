[android-components](../../index.md) / [mozilla.components.feature.pwa.feature](../index.md) / [WebAppActivityFeature](./index.md)

# WebAppActivityFeature

`class WebAppActivityFeature : LifecycleObserver` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/feature/WebAppActivityFeature.kt#L25)

Feature used to handle window effects for "standalone" and "fullscreen" web apps.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WebAppActivityFeature(activity: <ERROR CLASS>, icons: `[`BrowserIcons`](../../mozilla.components.browser.icons/-browser-icons/index.md)`, manifest: `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`)`<br>Feature used to handle window effects for "standalone" and "fullscreen" web apps. |

### Functions

| Name | Summary |
|---|---|
| [onDestroy](on-destroy.md) | `fun onDestroy(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onResume](on-resume.md) | `fun onResume(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
