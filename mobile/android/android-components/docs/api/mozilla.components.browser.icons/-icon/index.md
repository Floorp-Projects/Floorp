[android-components](../../index.md) / [mozilla.components.browser.icons](../index.md) / [Icon](./index.md)

# Icon

`data class Icon` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/Icon.kt#L16)

An [Icon](./index.md) returned by [BrowserIcons](../-browser-icons/index.md) after processing an [IconRequest](../-icon-request/index.md)

### Types

| Name | Summary |
|---|---|
| [Source](-source/index.md) | `enum class Source`<br>The source of an [Icon](./index.md). |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Icon(bitmap: <ERROR CLASS>, color: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`? = null, source: `[`Source`](-source/index.md)`)`<br>An [Icon](./index.md) returned by [BrowserIcons](../-browser-icons/index.md) after processing an [IconRequest](../-icon-request/index.md) |

### Properties

| Name | Summary |
|---|---|
| [bitmap](bitmap.md) | `val bitmap: <ERROR CLASS>`<br>The loaded icon as a [Bitmap](#). |
| [color](color.md) | `val color: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?`<br>The dominant color of the icon. Will be null if no color could be extracted. |
| [source](source.md) | `val source: `[`Source`](-source/index.md)<br>The source of the icon. |
