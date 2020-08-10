[android-components](../index.md) / [mozilla.components.support.ktx.android.view](index.md) / [showKeyboard](./show-keyboard.md)

# showKeyboard

`fun <ERROR CLASS>.showKeyboard(flags: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = InputMethodManager.SHOW_IMPLICIT): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/android/view/View.kt#L42)

Tries to focus this view and show the soft input window for it.

### Parameters

`flags` - Provides additional operating flags to be used with InputMethodManager.showSoftInput().
Currently may be 0, SHOW_IMPLICIT or SHOW_FORCED.