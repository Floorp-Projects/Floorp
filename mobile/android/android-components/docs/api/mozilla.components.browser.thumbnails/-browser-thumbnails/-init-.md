[android-components](../../index.md) / [mozilla.components.browser.thumbnails](../index.md) / [BrowserThumbnails](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`BrowserThumbnails(context: <ERROR CLASS>, engineView: `[`EngineView`](../../mozilla.components.concept.engine/-engine-view/index.md)`, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`)`

Feature implementation for automatically taking thumbnails of sites.
The feature will take a screenshot when the page finishes loading,
and will add it to the [ContentState.thumbnail](../../mozilla.components.browser.state.state/-content-state/thumbnail.md) property.

If the OS is under low memory conditions, the screenshot will be not taken.
Ideally, this should be used in conjunction with `SessionManager.onLowMemory` to allow
free up some [ContentState.thumbnail](../../mozilla.components.browser.state.state/-content-state/thumbnail.md) from memory.

