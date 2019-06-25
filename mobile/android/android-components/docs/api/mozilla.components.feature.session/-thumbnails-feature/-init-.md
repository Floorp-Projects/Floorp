[android-components](../../index.md) / [mozilla.components.feature.session](../index.md) / [ThumbnailsFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`ThumbnailsFeature(context: <ERROR CLASS>, engineView: `[`EngineView`](../../mozilla.components.concept.engine/-engine-view/index.md)`, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`)`

Feature implementation for automatically taking thumbnails of sites.
The feature will take a screenshot when the page finishes loading,
and will add it to the [Session.thumbnail](../../mozilla.components.browser.session/-session/thumbnail.md) property.

If the OS is under low memory conditions, the screenshot will be not taken.
Ideally, this should be used in conjunction with [SessionManager.onLowMemory](../../mozilla.components.browser.session/-session-manager/on-low-memory.md) to allow
free up some [Session.thumbnail](../../mozilla.components.browser.session/-session/thumbnail.md) from memory.

