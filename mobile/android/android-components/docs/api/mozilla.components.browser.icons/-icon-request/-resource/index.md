[android-components](../../../index.md) / [mozilla.components.browser.icons](../../index.md) / [IconRequest](../index.md) / [Resource](./index.md)

# Resource

`data class Resource` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/IconRequest.kt#L47)

An icon resource that can be loaded.

### Parameters

`url` - URL the icon resource can be fetched from.

`type` - The type of the icon.

`sizes` - Optional list of icon sizes provided by this resource (if known).

`mimeType` - Optional MIME type of this icon resource (if known).

`maskable` - True if the icon represents as full-bleed icon that can be cropped to other shapes.

### Types

| Name | Summary |
|---|---|
| [Type](-type/index.md) | `enum class Type`<br>An icon resource type. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Resource(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, type: `[`Type`](-type/index.md)`, sizes: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Size`](../../../mozilla.components.concept.engine.manifest/-size/index.md)`> = emptyList(), mimeType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, maskable: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`<br>An icon resource that can be loaded. |

### Properties

| Name | Summary |
|---|---|
| [maskable](maskable.md) | `val maskable: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>True if the icon represents as full-bleed icon that can be cropped to other shapes. |
| [mimeType](mime-type.md) | `val mimeType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Optional MIME type of this icon resource (if known). |
| [sizes](sizes.md) | `val sizes: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Size`](../../../mozilla.components.concept.engine.manifest/-size/index.md)`>`<br>Optional list of icon sizes provided by this resource (if known). |
| [type](type.md) | `val type: `[`Type`](-type/index.md)<br>The type of the icon. |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>URL the icon resource can be fetched from. |
