[android-components](../../index.md) / [mozilla.components.browser.icons.generator](../index.md) / [IconGenerator](./index.md)

# IconGenerator

`interface IconGenerator` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/generator/IconGenerator.kt#L16)

A [IconGenerator](./index.md) implementation can generate a [Bitmap](#) for an [IconRequest](../../mozilla.components.browser.icons/-icon-request/index.md). It's a fallback if no icon could be
loaded for a specific URL.

### Functions

| Name | Summary |
|---|---|
| [generate](generate.md) | `abstract fun generate(context: <ERROR CLASS>, request: `[`IconRequest`](../../mozilla.components.browser.icons/-icon-request/index.md)`): `[`Icon`](../../mozilla.components.browser.icons/-icon/index.md) |

### Inheritors

| Name | Summary |
|---|---|
| [DefaultIconGenerator](../-default-icon-generator/index.md) | `class DefaultIconGenerator : `[`IconGenerator`](./index.md)<br>[IconGenerator](./index.md) implementation that will generate an icon with a background color, rounded corners and a letter representing the URL. |
