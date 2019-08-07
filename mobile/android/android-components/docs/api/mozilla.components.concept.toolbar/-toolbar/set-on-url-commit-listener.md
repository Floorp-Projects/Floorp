[android-components](../../index.md) / [mozilla.components.concept.toolbar](../index.md) / [Toolbar](index.md) / [setOnUrlCommitListener](./set-on-url-commit-listener.md)

# setOnUrlCommitListener

`abstract fun setOnUrlCommitListener(listener: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/toolbar/src/main/java/mozilla/components/concept/toolbar/Toolbar.kt#L80)

Registers the given function to be invoked when the user selected a new URL i.e. is done
editing.

If the function returns `true` then the toolbar will automatically switch to "display mode". Otherwise it
remains in "edit mode".

### Parameters

`listener` - the listener function