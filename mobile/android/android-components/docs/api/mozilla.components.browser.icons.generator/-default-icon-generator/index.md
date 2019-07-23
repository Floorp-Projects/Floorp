[android-components](../../index.md) / [mozilla.components.browser.icons.generator](../index.md) / [DefaultIconGenerator](./index.md)

# DefaultIconGenerator

`class DefaultIconGenerator : `[`IconGenerator`](../-icon-generator/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/generator/DefaultIconGenerator.kt#L31)

[IconGenerator](../-icon-generator/index.md) implementation that will generate an icon with a background color, rounded corners and a letter
representing the URL.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DefaultIconGenerator(cornerRadiusDimen: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`? = R.dimen.mozac_browser_icons_generator_default_corner_radius, textColorRes: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = R.color.mozac_browser_icons_generator_default_text_color, backgroundColorsRes: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = R.array.mozac_browser_icons_photon_palette)`<br>[IconGenerator](../-icon-generator/index.md) implementation that will generate an icon with a background color, rounded corners and a letter representing the URL. |

### Functions

| Name | Summary |
|---|---|
| [generate](generate.md) | `fun generate(context: <ERROR CLASS>, request: `[`IconRequest`](../../mozilla.components.browser.icons/-icon-request/index.md)`): `[`Icon`](../../mozilla.components.browser.icons/-icon/index.md) |
