[android-components](../../index.md) / [mozilla.components.browser.toolbar](../index.md) / [BrowserToolbar](index.md) / [onUrlClicked](./on-url-clicked.md)

# onUrlClicked

`var onUrlClicked: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/toolbar/src/main/java/mozilla/components/browser/toolbar/BrowserToolbar.kt#L172)

Sets a lambda that will be invoked whenever the URL in display mode was clicked. Only if this
lambda returns true the toolbar will switch to editing mode. Return
false to not switch to editing mode and handle the click manually.

