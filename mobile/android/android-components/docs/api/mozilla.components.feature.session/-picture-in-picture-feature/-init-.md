[android-components](../../index.md) / [mozilla.components.feature.session](../index.md) / [PictureInPictureFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`PictureInPictureFeature(store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, activity: <ERROR CLASS>, crashReporting: `[`CrashReporting`](../../mozilla.components.support.base.crash/-crash-reporting/index.md)`? = null, tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null)`

A simple implementation of Picture-in-picture mode if on a supported platform.

### Parameters

`store` - Browser Store for observing the selected session's fullscreen mode changes.

`activity` - the activity with the EngineView for calling PIP mode when required; the AndroidX Fragment
doesn't support this.

`crashReporting` - Instance of `CrashReporting` to record unexpected caught exceptions

`tabId` - ID of tab or custom tab session.