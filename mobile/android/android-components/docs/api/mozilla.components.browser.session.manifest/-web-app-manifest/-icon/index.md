[android-components](../../../index.md) / [mozilla.components.browser.session.manifest](../../index.md) / [WebAppManifest](../index.md) / [Icon](./index.md)

# Icon

`data class Icon` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/manifest/WebAppManifest.kt#L97)

An image file that can serve as application icon.

### Types

| Name | Summary |
|---|---|
| [Purpose](-purpose/index.md) | `enum class Purpose` |
| [Size](-size/index.md) | `data class Size` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Icon(src: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, sizes: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Size`](-size/index.md)`>, type: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, purpose: `[`Purpose`](-purpose/index.md)` = Purpose.ANY)`<br>An image file that can serve as application icon. |

### Properties

| Name | Summary |
|---|---|
| [purpose](purpose.md) | `val purpose: `[`Purpose`](-purpose/index.md)<br>Defines the purpose of the image, for example that the image is intended to serve some special purpose in the context of the host OS (i.e., for better integration). |
| [sizes](sizes.md) | `val sizes: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Size`](-size/index.md)`>`<br>A list of image dimensions. |
| [src](src.md) | `val src: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The path to the image file. If src is a relative URL, the base URL will be the URL of the manifest. |
| [type](type.md) | `val type: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>A hint as to the media type of the image. The purpose of this member is to allow a user agent to quickly ignore images of media types it does not support. |
