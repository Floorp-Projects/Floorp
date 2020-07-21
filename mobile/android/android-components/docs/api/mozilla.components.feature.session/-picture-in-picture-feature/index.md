[android-components](../../index.md) / [mozilla.components.feature.session](../index.md) / [PictureInPictureFeature](./index.md)

# PictureInPictureFeature

`class PictureInPictureFeature` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/PictureInPictureFeature.kt#L28)

A simple implementation of Picture-in-picture mode if on a supported platform.

### Parameters

`store` - Browser Store for observing the selected session's fullscreen mode changes.

`activity` - the activity with the EngineView for calling PIP mode when required; the AndroidX Fragment
doesn't support this.

`crashReporting` - Instance of `CrashReporting` to record unexpected caught exceptions

`tabId` - ID of tab or custom tab session.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `PictureInPictureFeature(store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, activity: <ERROR CLASS>, crashReporting: `[`CrashReporting`](../../mozilla.components.support.base.crash/-crash-reporting/index.md)`? = null, tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null)`<br>A simple implementation of Picture-in-picture mode if on a supported platform. |

### Functions

| Name | Summary |
|---|---|
| [enterPipModeCompat](enter-pip-mode-compat.md) | `fun enterPipModeCompat(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Enter Picture-in-Picture mode. |
| [onHomePressed](on-home-pressed.md) | `fun onHomePressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onPictureInPictureModeChanged](on-picture-in-picture-mode-changed.md) | `fun onPictureInPictureModeChanged(enabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Should be called when the system informs you of changes to and from picture-in-picture mode. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
