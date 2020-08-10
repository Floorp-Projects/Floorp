[android-components](../index.md) / [mozilla.components.support.ktx.android.widget](index.md) / [adjustMaxTextSize](./adjust-max-text-size.md)

# adjustMaxTextSize

`fun <ERROR CLASS>.adjustMaxTextSize(heightMeasureSpec: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, ascenderPadding: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = DEFAULT_FONT_PADDING): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/android/widget/TextView.kt#L19)

Adjusts the text size of the [TextView](#) according to the height restriction given to the
[View.MeasureSpec](#) given in the parameter.

This will take [TextView.getIncludeFontPadding](#) into account when calculating the available height

