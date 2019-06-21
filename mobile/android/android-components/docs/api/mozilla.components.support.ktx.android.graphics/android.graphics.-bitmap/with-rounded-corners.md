[android-components](../../index.md) / [mozilla.components.support.ktx.android.graphics](../index.md) / [android.graphics.Bitmap](index.md) / [withRoundedCorners](./with-rounded-corners.md)

# withRoundedCorners

`@CheckResult fun `[`Bitmap`](https://developer.android.com/reference/android/graphics/Bitmap.html)`.withRoundedCorners(cornerRadiusPx: `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html)`): `[`Bitmap`](https://developer.android.com/reference/android/graphics/Bitmap.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/android/graphics/Bitmap.kt#L38)

Returns a new bitmap that is the receiver Bitmap with four rounded corners;
the receiver is unmodified.

This operation is expensive: it requires allocating an identical Bitmap and copying
all of the Bitmap's pixels. Consider these theoretically cheaper alternatives:

* android:background= a drawable with rounded corners
* Wrap your bitmap's ImageView with a layout that masks your view with rounded corners (e.g. CardView)
