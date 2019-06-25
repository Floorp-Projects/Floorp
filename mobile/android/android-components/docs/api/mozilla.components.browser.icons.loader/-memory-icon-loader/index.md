[android-components](../../index.md) / [mozilla.components.browser.icons.loader](../index.md) / [MemoryIconLoader](./index.md)

# MemoryIconLoader

`class MemoryIconLoader : `[`IconLoader`](../-icon-loader/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/loader/MemoryIconLoader.kt#L15)

An [IconLoader](../-icon-loader/index.md) implementation that loads icons from an in-memory cache.

### Types

| Name | Summary |
|---|---|
| [LoaderMemoryCache](-loader-memory-cache/index.md) | `interface LoaderMemoryCache` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `MemoryIconLoader(cache: `[`LoaderMemoryCache`](-loader-memory-cache/index.md)`)`<br>An [IconLoader](../-icon-loader/index.md) implementation that loads icons from an in-memory cache. |

### Functions

| Name | Summary |
|---|---|
| [load](load.md) | `fun load(context: <ERROR CLASS>, request: `[`IconRequest`](../../mozilla.components.browser.icons/-icon-request/index.md)`, resource: `[`Resource`](../../mozilla.components.browser.icons/-icon-request/-resource/index.md)`): `[`Result`](../-icon-loader/-result/index.md)<br>Tries to load the [IconRequest.Resource](../../mozilla.components.browser.icons/-icon-request/-resource/index.md) for the given [IconRequest](../../mozilla.components.browser.icons/-icon-request/index.md). |
