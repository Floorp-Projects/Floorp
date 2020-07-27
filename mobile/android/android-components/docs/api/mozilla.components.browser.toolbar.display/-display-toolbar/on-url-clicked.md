[android-components](../../index.md) / [mozilla.components.browser.toolbar.display](../index.md) / [DisplayToolbar](index.md) / [onUrlClicked](./on-url-clicked.md)

# onUrlClicked

`var onUrlClicked: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/toolbar/src/main/java/mozilla/components/browser/toolbar/display/DisplayToolbar.kt#L334)

Sets a lambda that will be invoked whenever the URL in display mode was clicked. Only if this
lambda returns true the toolbar will switch to editing mode. Return
false to not switch to editing mode and handle the click manually.

