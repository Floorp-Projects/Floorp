[android-components](../../index.md) / [mozilla.components.support.images](../index.md) / [ImageRequest](./index.md)

# ImageRequest

`data class ImageRequest` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/images/src/main/java/mozilla/components/support/images/ImageRequest.kt#L15)

A request to load an image.

### Types

| Name | Summary |
|---|---|
| [Size](-size/index.md) | `enum class Size`<br>Supported sizes. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ImageRequest(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, size: `[`Size`](-size/index.md)` = Size.DEFAULT)`<br>A request to load an image. |

### Properties

| Name | Summary |
|---|---|
| [id](id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The id of the image to retrieve. |
| [size](size.md) | `val size: `[`Size`](-size/index.md)<br>The preferred size of the image that should be loaded. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
