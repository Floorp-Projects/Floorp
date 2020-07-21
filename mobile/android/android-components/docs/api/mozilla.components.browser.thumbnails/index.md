[android-components](../index.md) / [mozilla.components.browser.thumbnails](./index.md)

## Package mozilla.components.browser.thumbnails

### Types

| Name | Summary |
|---|---|
| [BrowserThumbnails](-browser-thumbnails/index.md) | `class BrowserThumbnails : `[`LifecycleAwareFeature`](../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)<br>Feature implementation for automatically taking thumbnails of sites. The feature will take a screenshot when the page finishes loading, and will add it to the [ContentState.thumbnail](../mozilla.components.browser.state.state/-content-state/thumbnail.md) property. |
| [ThumbnailsMiddleware](-thumbnails-middleware/index.md) | `class ThumbnailsMiddleware : `[`Middleware`](../mozilla.components.lib.state/-middleware.md)`<`[`BrowserState`](../mozilla.components.browser.state.state/-browser-state/index.md)`, `[`BrowserAction`](../mozilla.components.browser.state.action/-browser-action.md)`>`<br>[Middleware](../mozilla.components.lib.state/-middleware.md) implementation for handling [ContentAction.UpdateThumbnailAction](../mozilla.components.browser.state.action/-content-action/-update-thumbnail-action/index.md) and storing the thumbnail to the disk cache. |
