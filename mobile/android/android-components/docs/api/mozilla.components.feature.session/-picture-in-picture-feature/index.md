[android-components](../../index.md) / [mozilla.components.feature.session](../index.md) / [PictureInPictureFeature](./index.md)

# PictureInPictureFeature

`class PictureInPictureFeature` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/PictureInPictureFeature.kt#L22)

A simple implementation of Picture-in-picture mode if on a supported platform.

### Parameters

`sessionManager` - Session Manager for observing the selected session's fullscreen mode changes.

`activity` - the activity with the EngineView for calling PIP mode when required; the AndroidX Fragment
doesn't support this.

`pipChanged` - a change listener that allows the calling app to perform changes based on PIP mode.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `PictureInPictureFeature(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, activity: <ERROR CLASS>, pipChanged: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`? = null)`<br>A simple implementation of Picture-in-picture mode if on a supported platform. |

### Functions

| Name | Summary |
|---|---|
| [enterPipModeCompat](enter-pip-mode-compat.md) | `fun enterPipModeCompat(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onHomePressed](on-home-pressed.md) | `fun onHomePressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onPictureInPictureModeChanged](on-picture-in-picture-mode-changed.md) | `fun onPictureInPictureModeChanged(enabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`?` |
