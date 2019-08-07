[android-components](../../index.md) / [mozilla.components.browser.toolbar](../index.md) / [BrowserToolbar](index.md) / [setAutocompleteListener](./set-autocomplete-listener.md)

# setAutocompleteListener

`fun setAutocompleteListener(filter: suspend (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`AutocompleteDelegate`](../../mozilla.components.concept.toolbar/-autocomplete-delegate/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/toolbar/src/main/java/mozilla/components/browser/toolbar/BrowserToolbar.kt#L362)

Overrides [Toolbar.setAutocompleteListener](../../mozilla.components.concept.toolbar/-toolbar/set-autocomplete-listener.md)

Registers the given function to be invoked when users changes text in the toolbar.

### Parameters

`filter` - A function which will perform autocompletion and send results to [AutocompleteDelegate](../../mozilla.components.concept.toolbar/-autocomplete-delegate/index.md).