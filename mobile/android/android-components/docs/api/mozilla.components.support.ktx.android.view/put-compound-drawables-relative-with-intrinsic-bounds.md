[android-components](../index.md) / [mozilla.components.support.ktx.android.view](index.md) / [putCompoundDrawablesRelativeWithIntrinsicBounds](./put-compound-drawables-relative-with-intrinsic-bounds.md)

# putCompoundDrawablesRelativeWithIntrinsicBounds

`inline fun <ERROR CLASS>.putCompoundDrawablesRelativeWithIntrinsicBounds(start: <ERROR CLASS>? = null, top: <ERROR CLASS>? = null, end: <ERROR CLASS>? = null, bottom: <ERROR CLASS>? = null): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/android/view/TextView.kt#L36)

Sets the [Drawable](#)s (if any) to appear to the start of, above, to the end of,
and below the text. Use `null` if you do not want a Drawable there.
The Drawables' bounds will be set to their intrinsic bounds.

Calling this method will overwrite any Drawables previously set using
[TextView.setCompoundDrawables](#) or related methods.

