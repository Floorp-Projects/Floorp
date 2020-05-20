[android-components](../../index.md) / [mozilla.components.browser.icons.processor](../index.md) / [DiskIconProcessor](./index.md)

# DiskIconProcessor

`class DiskIconProcessor : `[`IconProcessor`](../-icon-processor/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/processor/DiskIconProcessor.kt#L16)

[IconProcessor](../-icon-processor/index.md) implementation that saves icons in the disk cache.

### Types

| Name | Summary |
|---|---|
| [ProcessorDiskCache](-processor-disk-cache/index.md) | `interface ProcessorDiskCache` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DiskIconProcessor(cache: `[`ProcessorDiskCache`](-processor-disk-cache/index.md)`)`<br>[IconProcessor](../-icon-processor/index.md) implementation that saves icons in the disk cache. |

### Functions

| Name | Summary |
|---|---|
| [process](process.md) | `fun process(context: <ERROR CLASS>, request: `[`IconRequest`](../../mozilla.components.browser.icons/-icon-request/index.md)`, resource: `[`Resource`](../../mozilla.components.browser.icons/-icon-request/-resource/index.md)`?, icon: `[`Icon`](../../mozilla.components.browser.icons/-icon/index.md)`, desiredSize: `[`DesiredSize`](../../mozilla.components.support.images/-desired-size/index.md)`): `[`Icon`](../../mozilla.components.browser.icons/-icon/index.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
