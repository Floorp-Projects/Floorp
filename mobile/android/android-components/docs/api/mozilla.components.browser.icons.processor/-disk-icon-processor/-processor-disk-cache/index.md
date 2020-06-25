[android-components](../../../index.md) / [mozilla.components.browser.icons.processor](../../index.md) / [DiskIconProcessor](../index.md) / [ProcessorDiskCache](./index.md)

# ProcessorDiskCache

`interface ProcessorDiskCache` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/processor/DiskIconProcessor.kt#L19)

### Functions

| Name | Summary |
|---|---|
| [putIcon](put-icon.md) | `abstract fun putIcon(context: <ERROR CLASS>, resource: `[`Resource`](../../../mozilla.components.browser.icons/-icon-request/-resource/index.md)`, icon: `[`Icon`](../../../mozilla.components.browser.icons/-icon/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Saves icon bitmap to cache. |
| [putResources](put-resources.md) | `abstract fun putResources(context: <ERROR CLASS>, request: `[`IconRequest`](../../../mozilla.components.browser.icons/-icon-request/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Saves icon resources to cache. |

### Inheritors

| Name | Summary |
|---|---|
| [IconDiskCache](../../../mozilla.components.browser.icons.utils/-icon-disk-cache/index.md) | `class IconDiskCache : `[`LoaderDiskCache`](../../../mozilla.components.browser.icons.loader/-disk-icon-loader/-loader-disk-cache/index.md)`, `[`PreparerDiskCache`](../../../mozilla.components.browser.icons.preparer/-disk-icon-preparer/-preparer-disk-cache/index.md)`, `[`ProcessorDiskCache`](./index.md)<br>Caching bitmaps and resource URLs on disk. |
