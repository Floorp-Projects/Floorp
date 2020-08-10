[android-components](../../../index.md) / [mozilla.components.browser.icons](../../index.md) / [IconRequest](../index.md) / [Resource](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`Resource(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, type: `[`Type`](-type/index.md)`, sizes: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Size`](../../../mozilla.components.concept.engine.manifest/-size/index.md)`> = emptyList(), mimeType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, maskable: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`

An icon resource that can be loaded.

### Parameters

`url` - URL the icon resource can be fetched from.

`type` - The type of the icon.

`sizes` - Optional list of icon sizes provided by this resource (if known).

`mimeType` - Optional MIME type of this icon resource (if known).

`maskable` - True if the icon represents as full-bleed icon that can be cropped to other shapes.