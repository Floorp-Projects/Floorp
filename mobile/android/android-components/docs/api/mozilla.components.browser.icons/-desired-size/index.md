[android-components](../../index.md) / [mozilla.components.browser.icons](../index.md) / [DesiredSize](./index.md)

# DesiredSize

`data class DesiredSize` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/DesiredSize.kt#L18)

Represents the desired size of an icon loaded by [BrowserIcons](../-browser-icons/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DesiredSize(targetSize: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, maxSize: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, maxScaleFactor: `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html)`)`<br>Represents the desired size of an icon loaded by [BrowserIcons](../-browser-icons/index.md). |

### Properties

| Name | Summary |
|---|---|
| [maxScaleFactor](max-scale-factor.md) | `val maxScaleFactor: `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html)<br>The factor that the image can be scaled up before being thrown out. A lower scale factor results in less pixelation. |
| [maxSize](max-size.md) | `val maxSize: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>The maximum size of an image before it will be thrown out, in pixels. Extremely large images are suspicious and should be ignored. |
| [targetSize](target-size.md) | `val targetSize: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>The size the image will be displayed at, in pixels. |
