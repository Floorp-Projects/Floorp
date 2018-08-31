---
title: BrowserToolbar.Button.<init> - 
---

[mozilla.components.browser.toolbar](../../index.html) / [BrowserToolbar](../index.html) / [Button](index.html) / [&lt;init&gt;](./-init-.html)

# &lt;init&gt;

`Button(imageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true }, @DrawableRes background: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`? = null, listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`

An action button to be added to the toolbar.

### Parameters

`imageResource` - The drawable to be shown.

`contentDescription` - The content description to use.

`visible` - Lambda that returns true or false to indicate whether this button should be shown.

`background` - A custom (stateful) background drawable resource to be used.

`listener` - Callback that will be invoked whenever the button is pressed