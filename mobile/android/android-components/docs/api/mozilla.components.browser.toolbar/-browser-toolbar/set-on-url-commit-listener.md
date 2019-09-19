[android-components](../../index.md) / [mozilla.components.browser.toolbar](../index.md) / [BrowserToolbar](index.md) / [setOnUrlCommitListener](./set-on-url-commit-listener.md)

# setOnUrlCommitListener

`fun setOnUrlCommitListener(listener: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/toolbar/src/main/java/mozilla/components/browser/toolbar/BrowserToolbar.kt#L572)

Overrides [Toolbar.setOnUrlCommitListener](../../mozilla.components.concept.toolbar/-toolbar/set-on-url-commit-listener.md)

Registers the given function to be invoked when the user selected a new URL i.e. is done
editing.

If the function returns `true` then the toolbar will automatically switch to "display mode". Otherwise it
remains in "edit mode".

### Parameters

`listener` - the listener function