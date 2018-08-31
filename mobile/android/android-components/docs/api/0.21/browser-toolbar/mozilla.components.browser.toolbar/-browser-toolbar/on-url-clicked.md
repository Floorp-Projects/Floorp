---
title: BrowserToolbar.onUrlClicked - 
---

[mozilla.components.browser.toolbar](../index.html) / [BrowserToolbar](index.html) / [onUrlClicked](./on-url-clicked.html)

# onUrlClicked

`var onUrlClicked: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)

Sets a lambda that will be invoked whenever the URL in display mode was clicked. Only if this
lambda returns true the toolbar will switch to editing mode. Return
false to not switch to editing mode and handle the click manually.

