[android-components](../../index.md) / [mozilla.components.browser.icons.processor](../index.md) / [AdaptiveIconProcessor](./index.md)

# AdaptiveIconProcessor

`class AdaptiveIconProcessor : `[`IconProcessor`](../-icon-processor/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/processor/AdaptiveIconProcessor.kt#L23)

[IconProcessor](../-icon-processor/index.md) implementation that builds maskable icons.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AdaptiveIconProcessor()`<br>[IconProcessor](../-icon-processor/index.md) implementation that builds maskable icons. |

### Functions

| Name | Summary |
|---|---|
| [process](process.md) | `fun process(context: <ERROR CLASS>, request: `[`IconRequest`](../../mozilla.components.browser.icons/-icon-request/index.md)`, resource: `[`Resource`](../../mozilla.components.browser.icons/-icon-request/-resource/index.md)`?, icon: `[`Icon`](../../mozilla.components.browser.icons/-icon/index.md)`, desiredSize: `[`DesiredSize`](../../mozilla.components.support.images/-desired-size/index.md)`): `[`Icon`](../../mozilla.components.browser.icons/-icon/index.md)<br>Creates an adaptive icon using the base icon. On older devices, non-maskable icons are not transformed. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
