[android-components](../index.md) / [mozilla.components.support.ktx.android.view](index.md) / [putCompoundDrawablesRelative](./put-compound-drawables-relative.md)

# putCompoundDrawablesRelative

`inline fun <ERROR CLASS>.putCompoundDrawablesRelative(start: <ERROR CLASS>? = null, top: <ERROR CLASS>? = null, end: <ERROR CLASS>? = null, bottom: <ERROR CLASS>? = null): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/android/view/TextView.kt#L20)

Sets the [Drawable](#)s (if any) to appear to the start of, above, to the end of,
and below the text. Use `null` if you do not want a Drawable there.
The Drawables must already have had [Drawable.setBounds](#) called.

Calling this method will overwrite any Drawables previously set using
[TextView.setCompoundDrawables](#) or related methods.

