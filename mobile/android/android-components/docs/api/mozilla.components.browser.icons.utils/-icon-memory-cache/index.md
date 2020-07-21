[android-components](../../index.md) / [mozilla.components.browser.icons.utils](../index.md) / [IconMemoryCache](./index.md)

# IconMemoryCache

`class IconMemoryCache : `[`ProcessorMemoryCache`](../../mozilla.components.browser.icons.processor/-memory-icon-processor/-processor-memory-cache/index.md)`, `[`LoaderMemoryCache`](../../mozilla.components.browser.icons.loader/-memory-icon-loader/-loader-memory-cache/index.md)`, `[`PreparerMemoryCache`](../../mozilla.components.browser.icons.preparer/-memory-icon-preparer/-preparer-memory-cache/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/utils/IconMemoryCache.kt#L21)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `IconMemoryCache()` |

### Functions

| Name | Summary |
|---|---|
| [getBitmap](get-bitmap.md) | `fun getBitmap(request: `[`IconRequest`](../../mozilla.components.browser.icons/-icon-request/index.md)`, resource: `[`Resource`](../../mozilla.components.browser.icons/-icon-request/-resource/index.md)`): <ERROR CLASS>?` |
| [getResources](get-resources.md) | `fun getResources(request: `[`IconRequest`](../../mozilla.components.browser.icons/-icon-request/index.md)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Resource`](../../mozilla.components.browser.icons/-icon-request/-resource/index.md)`>` |
| [put](put.md) | `fun put(request: `[`IconRequest`](../../mozilla.components.browser.icons/-icon-request/index.md)`, resource: `[`Resource`](../../mozilla.components.browser.icons/-icon-request/-resource/index.md)`, icon: `[`Icon`](../../mozilla.components.browser.icons/-icon/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
