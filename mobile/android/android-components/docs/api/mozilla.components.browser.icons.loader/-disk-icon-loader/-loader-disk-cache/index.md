[android-components](../../../index.md) / [mozilla.components.browser.icons.loader](../../index.md) / [DiskIconLoader](../index.md) / [LoaderDiskCache](./index.md)

# LoaderDiskCache

`interface LoaderDiskCache` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/loader/DiskIconLoader.kt#L17)

### Functions

| Name | Summary |
|---|---|
| [getIconData](get-icon-data.md) | `abstract fun getIconData(context: <ERROR CLASS>, resource: `[`Resource`](../../../mozilla.components.browser.icons/-icon-request/-resource/index.md)`): `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`?` |

### Inheritors

| Name | Summary |
|---|---|
| [IconDiskCache](../../../mozilla.components.browser.icons.utils/-icon-disk-cache/index.md) | `class IconDiskCache : `[`LoaderDiskCache`](./index.md)`, `[`PreparerDiskCache`](../../../mozilla.components.browser.icons.preparer/-disk-icon-preparer/-preparer-disk-cache/index.md)`, `[`ProcessorDiskCache`](../../../mozilla.components.browser.icons.processor/-disk-icon-processor/-processor-disk-cache/index.md)<br>Caching bitmaps and resource URLs on disk. |
