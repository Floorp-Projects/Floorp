[android-components](../../index.md) / [mozilla.components.concept.toolbar](../index.md) / [Toolbar](index.md) / [setAutocompleteListener](./set-autocomplete-listener.md)

# setAutocompleteListener

`abstract fun setAutocompleteListener(filter: suspend (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`AutocompleteDelegate`](../-autocomplete-delegate/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/toolbar/src/main/java/mozilla/components/concept/toolbar/Toolbar.kt#L92)

Registers the given function to be invoked when users changes text in the toolbar.

### Parameters

`filter` - A function which will perform autocompletion and send results to [AutocompleteDelegate](../-autocomplete-delegate/index.md).