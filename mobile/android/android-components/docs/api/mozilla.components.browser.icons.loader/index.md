[android-components](../index.md) / [mozilla.components.browser.icons.loader](./index.md)

## Package mozilla.components.browser.icons.loader

### Types

| Name | Summary |
|---|---|
| [DataUriIconLoader](-data-uri-icon-loader/index.md) | `class DataUriIconLoader : `[`IconLoader`](-icon-loader/index.md)<br>An [IconLoader](-icon-loader/index.md) implementation that will base64 decode the image bytes from a data:image uri. |
| [DiskIconLoader](-disk-icon-loader/index.md) | `class DiskIconLoader : `[`IconLoader`](-icon-loader/index.md)<br>[IconLoader](-icon-loader/index.md) implementation that loads icons from a disk cache. |
| [HttpIconLoader](-http-icon-loader/index.md) | `class HttpIconLoader : `[`IconLoader`](-icon-loader/index.md)<br>[IconLoader](-icon-loader/index.md) implementation that will try to download the icon for resources that point to an http(s) URL. |
| [IconLoader](-icon-loader/index.md) | `interface IconLoader`<br>A loader that can load an icon from an [IconRequest.Resource](../mozilla.components.browser.icons/-icon-request/-resource/index.md). |
| [MemoryIconLoader](-memory-icon-loader/index.md) | `class MemoryIconLoader : `[`IconLoader`](-icon-loader/index.md)<br>An [IconLoader](-icon-loader/index.md) implementation that loads icons from an in-memory cache. |
