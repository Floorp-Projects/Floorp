---
title: showKeyboard - 
---

[mozilla.components.support.ktx.android.view](../index.html) / [android.view.View](index.html) / [showKeyboard](./show-keyboard.html)

# showKeyboard

`fun View.showKeyboard(flags: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = InputMethodManager.SHOW_IMPLICIT): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)

Tries to focus this view and show the soft input window for it.

### Parameters

`flags` - Provides additional operating flags to be used with InputMethodManager.showSoftInput().
Currently may be 0, SHOW_IMPLICIT or SHOW_FORCED.