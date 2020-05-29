[android-components](../../../index.md) / [mozilla.components.browser.thumbnails](../../index.md) / [ThumbnailsUseCases](../index.md) / [LoadThumbnailUseCase](./index.md)

# LoadThumbnailUseCase

`class LoadThumbnailUseCase` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/thumbnails/src/main/java/mozilla/components/browser/thumbnails/ThumbnailsUseCases.kt#L23)

Load thumbnail use case.

### Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `operator suspend fun invoke(sessionIdOrUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): <ERROR CLASS>?`<br>Loads the thumbnail of a tab from its in-memory [ContentState](#) or from the disk cache of [ThumbnailStorage](../../../mozilla.components.browser.thumbnails.storage/-thumbnail-storage/index.md). |
