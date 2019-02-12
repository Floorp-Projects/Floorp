[android-components](../../index.md) / [mozilla.components.browser.toolbar](../index.md) / [BrowserToolbar](index.md) / [setOnUrlCommitListener](./set-on-url-commit-listener.md)

# setOnUrlCommitListener

`fun setOnUrlCommitListener(listener: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/toolbar/src/main/java/mozilla/components/browser/toolbar/BrowserToolbar.kt#L325)

Overrides [Toolbar.setOnUrlCommitListener](../../mozilla.components.concept.toolbar/-toolbar/set-on-url-commit-listener.md)

Registers the given function to be invoked when the user selected a new URL i.e. is done
editing.

### Parameters

`listener` - the listener function