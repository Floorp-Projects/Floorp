[android-components](../../index.md) / [mozilla.components.feature.session](../index.md) / [ThumbnailsFeature](./index.md)

# ThumbnailsFeature

`class ThumbnailsFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/ThumbnailsFeature.kt#L25)

Feature implementation for automatically taking thumbnails of sites.
The feature will take a screenshot when the page finishes loading,
and will add it to the [Session.thumbnail](../../mozilla.components.browser.session/-session/thumbnail.md) property.

If the OS is under low memory conditions, the screenshot will be not taken.
Ideally, this should be used in conjunction with [SessionManager.onLowMemory](../../mozilla.components.browser.session/-session-manager/on-low-memory.md) to allow
free up some [Session.thumbnail](../../mozilla.components.browser.session/-session/thumbnail.md) from memory.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ThumbnailsFeature(context: <ERROR CLASS>, engineView: `[`EngineView`](../../mozilla.components.concept.engine/-engine-view/index.md)`, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`)`<br>Feature implementation for automatically taking thumbnails of sites. The feature will take a screenshot when the page finishes loading, and will add it to the [Session.thumbnail](../../mozilla.components.browser.session/-session/thumbnail.md) property. |

### Functions

| Name | Summary |
|---|---|
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing the selected session to listen for when a session finish loading. |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops observing the selected session. |
