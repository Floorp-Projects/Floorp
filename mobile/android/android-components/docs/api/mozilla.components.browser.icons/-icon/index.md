[android-components](../../index.md) / [mozilla.components.browser.icons](../index.md) / [Icon](./index.md)

# Icon

`data class Icon` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/Icon.kt#L17)

An [Icon](./index.md) returned by [BrowserIcons](../-browser-icons/index.md) after processing an [IconRequest](../-icon-request/index.md)

### Types

| Name | Summary |
|---|---|
| [Source](-source/index.md) | `enum class Source`<br>The source of an [Icon](./index.md). |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Icon(bitmap: <ERROR CLASS>, color: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`? = null, source: `[`Source`](-source/index.md)`, maskable: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`<br>An [Icon](./index.md) returned by [BrowserIcons](../-browser-icons/index.md) after processing an [IconRequest](../-icon-request/index.md) |

### Properties

| Name | Summary |
|---|---|
| [bitmap](bitmap.md) | `val bitmap: <ERROR CLASS>`<br>The loaded icon as a [Bitmap](#). |
| [color](color.md) | `val color: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?`<br>The dominant color of the icon. Will be null if no color could be extracted. |
| [maskable](maskable.md) | `val maskable: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>True if the icon represents as full-bleed icon that can be cropped to other shapes. |
| [source](source.md) | `val source: `[`Source`](-source/index.md)<br>The source of the icon. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
