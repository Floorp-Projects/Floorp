[android-components](../../../index.md) / [mozilla.components.browser.icons.processor](../../index.md) / [MemoryIconProcessor](../index.md) / [ProcessorMemoryCache](./index.md)

# ProcessorMemoryCache

`interface ProcessorMemoryCache` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/processor/MemoryIconProcessor.kt#L19)

### Functions

| Name | Summary |
|---|---|
| [put](put.md) | `abstract fun put(request: `[`IconRequest`](../../../mozilla.components.browser.icons/-icon-request/index.md)`, resource: `[`Resource`](../../../mozilla.components.browser.icons/-icon-request/-resource/index.md)`, icon: `[`Icon`](../../../mozilla.components.browser.icons/-icon/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [IconMemoryCache](../../../mozilla.components.browser.icons.utils/-icon-memory-cache/index.md) | `class IconMemoryCache : `[`ProcessorMemoryCache`](./index.md)`, `[`LoaderMemoryCache`](../../../mozilla.components.browser.icons.loader/-memory-icon-loader/-loader-memory-cache/index.md)`, `[`PreparerMemoryCache`](../../../mozilla.components.browser.icons.preparer/-memory-icon-preparer/-preparer-memory-cache/index.md) |
