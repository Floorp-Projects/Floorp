[android-components](../../index.md) / [mozilla.components.feature.session](../index.md) / [PictureInPictureFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`PictureInPictureFeature(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, activity: <ERROR CLASS>, pipChanged: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`? = null)`

A simple implementation of Picture-in-picture mode if on a supported platform.

### Parameters

`sessionManager` - Session Manager for observing the selected session's fullscreen mode changes.

`activity` - the activity with the EngineView for calling PIP mode when required; the AndroidX Fragment
doesn't support this.

`pipChanged` - a change listener that allows the calling app to perform changes based on PIP mode.