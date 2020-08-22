[android-components](../../index.md) / [mozilla.components.browser.thumbnails](../index.md) / [BrowserThumbnails](index.md) / [requestScreenshot](./request-screenshot.md)

# requestScreenshot

`fun requestScreenshot(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/thumbnails/src/main/java/mozilla/components/browser/thumbnails/BrowserThumbnails.kt#L59)

Requests a screenshot to be taken that can be observed from [BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md) if successful. The request can fail
if the device is low on memory or if there is no tab attached to the [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md).

