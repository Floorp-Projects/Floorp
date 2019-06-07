[android-components](../../index.md) / [mozilla.components.support.test.robolectric.shadow](../index.md) / [PixelCopyShadow](./index.md)

# PixelCopyShadow

`@Implements(PixelCopy, 24) class PixelCopyShadow` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/test/src/main/java/mozilla/components/support/test/robolectric/shadow/PixelCopyShadow.kt#L22)

Shadow for [PixelCopy](https://developer.android.com/reference/android/view/PixelCopy.html) API.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `PixelCopyShadow()`<br>Shadow for [PixelCopy](https://developer.android.com/reference/android/view/PixelCopy.html) API. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [copyResult](copy-result.md) | `var copyResult: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Companion Object Functions

| Name | Summary |
|---|---|
| [request](request.md) | `fun request(source: `[`Window`](https://developer.android.com/reference/android/view/Window.html)`, srcRect: `[`Rect`](https://developer.android.com/reference/android/graphics/Rect.html)`?, dest: `[`Bitmap`](https://developer.android.com/reference/android/graphics/Bitmap.html)`, listener: `[`OnPixelCopyFinishedListener`](https://developer.android.com/reference/android/view/PixelCopy/OnPixelCopyFinishedListener.html)`, listenerThread: `[`Handler`](https://developer.android.com/reference/android/os/Handler.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
