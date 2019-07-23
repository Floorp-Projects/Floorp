[android-components](../../index.md) / [mozilla.components.browser.icons.utils](../index.md) / [IconDiskCache](./index.md)

# IconDiskCache

`class IconDiskCache : `[`LoaderDiskCache`](../../mozilla.components.browser.icons.loader/-disk-icon-loader/-loader-disk-cache/index.md)`, `[`PreparerDiskCache`](../../mozilla.components.browser.icons.preparer/-disk-icon-preparer/-preparer-disk-cache/index.md)`, `[`ProcessorDiskCache`](../../mozilla.components.browser.icons.processor/-disk-icon-processor/-processor-disk-cache/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/utils/IconDiskCache.kt#L36)

Caching bitmaps and resource URLs on disk.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `IconDiskCache()`<br>Caching bitmaps and resource URLs on disk. |

### Functions

| Name | Summary |
|---|---|
| [getIconData](get-icon-data.md) | `fun getIconData(context: <ERROR CLASS>, resource: `[`Resource`](../../mozilla.components.browser.icons/-icon-request/-resource/index.md)`): `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`?` |
| [getResources](get-resources.md) | `fun getResources(context: <ERROR CLASS>, request: `[`IconRequest`](../../mozilla.components.browser.icons/-icon-request/index.md)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Resource`](../../mozilla.components.browser.icons/-icon-request/-resource/index.md)`>` |
| [put](put.md) | `fun put(context: <ERROR CLASS>, request: `[`IconRequest`](../../mozilla.components.browser.icons/-icon-request/index.md)`, resource: `[`Resource`](../../mozilla.components.browser.icons/-icon-request/-resource/index.md)`, icon: `[`Icon`](../../mozilla.components.browser.icons/-icon/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
