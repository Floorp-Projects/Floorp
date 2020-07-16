[android-components](../../index.md) / [mozilla.components.support.images](../index.md) / [ImageLoadRequest](./index.md)

# ImageLoadRequest

`data class ImageLoadRequest` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/images/src/main/java/mozilla/components/support/images/ImageRequest.kt#L20)

A request to load an image.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ImageLoadRequest(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, size: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`)`<br>A request to load an image. |

### Properties

| Name | Summary |
|---|---|
| [id](id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The id of the image to retrieve. |
| [size](size.md) | `val size: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>The preferred size of the image that should be loaded in pixels. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
