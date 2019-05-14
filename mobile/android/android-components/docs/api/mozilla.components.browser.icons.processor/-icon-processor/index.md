[android-components](../../index.md) / [mozilla.components.browser.icons.processor](../index.md) / [IconProcessor](./index.md)

# IconProcessor

`interface IconProcessor` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/processor/IconProcessor.kt#L15)

An [IconProcessor](./index.md) implementation receives the [Icon](../../mozilla.components.browser.icons/-icon/index.md) with the [IconRequest](../../mozilla.components.browser.icons/-icon-request/index.md) and [IconRequest.Resource](../../mozilla.components.browser.icons/-icon-request/-resource/index.md) after
the icon was loaded. The [IconProcessor](./index.md) has the option to rewrite a loaded [Icon](../../mozilla.components.browser.icons/-icon/index.md) and return a new instance.

### Functions

| Name | Summary |
|---|---|
| [process](process.md) | `abstract fun process(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, request: `[`IconRequest`](../../mozilla.components.browser.icons/-icon-request/index.md)`, resource: `[`Resource`](../../mozilla.components.browser.icons/-icon-request/-resource/index.md)`?, icon: `[`Icon`](../../mozilla.components.browser.icons/-icon/index.md)`): `[`Icon`](../../mozilla.components.browser.icons/-icon/index.md) |

### Inheritors

| Name | Summary |
|---|---|
| [DiskIconProcessor](../-disk-icon-processor/index.md) | `class DiskIconProcessor : `[`IconProcessor`](./index.md)<br>[IconProcessor](./index.md) implementation that saves icons in the disk cache. |
| [MemoryIconProcessor](../-memory-icon-processor/index.md) | `class MemoryIconProcessor : `[`IconProcessor`](./index.md)<br>An [IconProcessor](./index.md) implementation that saves icons in the in-memory cache. |
