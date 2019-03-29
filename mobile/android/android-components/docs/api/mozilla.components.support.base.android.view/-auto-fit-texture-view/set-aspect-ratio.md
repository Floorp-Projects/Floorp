[android-components](../../index.md) / [mozilla.components.support.base.android.view](../index.md) / [AutoFitTextureView](index.md) / [setAspectRatio](./set-aspect-ratio.md)

# setAspectRatio

`fun setAspectRatio(width: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, height: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/android/view/AutoFitTextureView.kt#L48)

Sets the aspect ratio for this view. The size of the view will be measured based on the ratio
calculated from the parameters. Note that the actual sizes of parameters don't matter, that
is, calling setAspectRatio(2, 3) and setAspectRatio(4, 6) make the same result.

### Parameters

`width` - Relative horizontal size

`height` - Relative vertical size