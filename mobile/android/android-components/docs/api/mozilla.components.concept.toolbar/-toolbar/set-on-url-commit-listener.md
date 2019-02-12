[android-components](../../index.md) / [mozilla.components.concept.toolbar](../index.md) / [Toolbar](index.md) / [setOnUrlCommitListener](./set-on-url-commit-listener.md)

# setOnUrlCommitListener

`abstract fun setOnUrlCommitListener(listener: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/toolbar/src/main/java/mozilla/components/concept/toolbar/Toolbar.kt#L59)

Registers the given function to be invoked when the user selected a new URL i.e. is done
editing.

### Parameters

`listener` - the listener function