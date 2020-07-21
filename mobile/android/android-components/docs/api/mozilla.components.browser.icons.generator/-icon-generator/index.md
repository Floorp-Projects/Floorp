[android-components](../../index.md) / [mozilla.components.browser.icons.generator](../index.md) / [IconGenerator](./index.md)

# IconGenerator

`interface IconGenerator` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/generator/IconGenerator.kt#L16)

A [IconGenerator](./index.md) implementation can generate a [Bitmap](#) for an [IconRequest](../../mozilla.components.browser.icons/-icon-request/index.md). It's a fallback if no icon could be
loaded for a specific URL.

### Functions

| Name | Summary |
|---|---|
| [generate](generate.md) | `abstract fun generate(context: <ERROR CLASS>, request: `[`IconRequest`](../../mozilla.components.browser.icons/-icon-request/index.md)`): `[`Icon`](../../mozilla.components.browser.icons/-icon/index.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [DefaultIconGenerator](../-default-icon-generator/index.md) | `class DefaultIconGenerator : `[`IconGenerator`](./index.md)<br>[IconGenerator](./index.md) implementation that will generate an icon with a background color, rounded corners and a letter representing the URL. |
