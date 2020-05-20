[android-components](../../index.md) / [mozilla.components.support.images](../index.md) / [DesiredSize](./index.md)

# DesiredSize

`data class DesiredSize` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/images/src/main/java/mozilla/components/support/images/DesiredSize.kt#L18)

Represents the desired size of an images to be loaded.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DesiredSize(targetSize: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, maxSize: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, maxScaleFactor: `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html)`)`<br>Represents the desired size of an images to be loaded. |

### Properties

| Name | Summary |
|---|---|
| [maxScaleFactor](max-scale-factor.md) | `val maxScaleFactor: `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html)<br>The factor that the image can be scaled up before being thrown out. A lower scale factor results in less pixelation. |
| [maxSize](max-size.md) | `val maxSize: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>The maximum size of an image before it will be thrown out, in pixels. Extremely large images are suspicious and should be ignored. |
| [targetSize](target-size.md) | `val targetSize: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>The size the image will be displayed at, in pixels. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
