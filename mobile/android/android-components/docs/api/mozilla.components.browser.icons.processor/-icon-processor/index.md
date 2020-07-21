[android-components](../../index.md) / [mozilla.components.browser.icons.processor](../index.md) / [IconProcessor](./index.md)

# IconProcessor

`interface IconProcessor` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/processor/IconProcessor.kt#L16)

An [IconProcessor](./index.md) implementation receives the [Icon](../../mozilla.components.browser.icons/-icon/index.md) with the [IconRequest](../../mozilla.components.browser.icons/-icon-request/index.md) and [IconRequest.Resource](../../mozilla.components.browser.icons/-icon-request/-resource/index.md) after
the icon was loaded. The [IconProcessor](./index.md) has the option to rewrite a loaded [Icon](../../mozilla.components.browser.icons/-icon/index.md) and return a new instance.

### Functions

| Name | Summary |
|---|---|
| [process](process.md) | `abstract fun process(context: <ERROR CLASS>, request: `[`IconRequest`](../../mozilla.components.browser.icons/-icon-request/index.md)`, resource: `[`Resource`](../../mozilla.components.browser.icons/-icon-request/-resource/index.md)`?, icon: `[`Icon`](../../mozilla.components.browser.icons/-icon/index.md)`, desiredSize: `[`DesiredSize`](../../mozilla.components.support.images/-desired-size/index.md)`): `[`Icon`](../../mozilla.components.browser.icons/-icon/index.md)`?` |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [AdaptiveIconProcessor](../-adaptive-icon-processor/index.md) | `class AdaptiveIconProcessor : `[`IconProcessor`](./index.md)<br>[IconProcessor](./index.md) implementation that builds maskable icons. |
| [ColorProcessor](../-color-processor/index.md) | `class ColorProcessor : `[`IconProcessor`](./index.md)<br>[IconProcessor](./index.md) implementation to extract the dominant color from the icon. |
| [DiskIconProcessor](../-disk-icon-processor/index.md) | `class DiskIconProcessor : `[`IconProcessor`](./index.md)<br>[IconProcessor](./index.md) implementation that saves icons in the disk cache. |
| [MemoryIconProcessor](../-memory-icon-processor/index.md) | `class MemoryIconProcessor : `[`IconProcessor`](./index.md)<br>An [IconProcessor](./index.md) implementation that saves icons in the in-memory cache. |
| [ResizingProcessor](../-resizing-processor/index.md) | `class ResizingProcessor : `[`IconProcessor`](./index.md)<br>[IconProcessor](./index.md) implementation for resizing the loaded icon based on the target size. |
