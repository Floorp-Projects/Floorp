[android-components](../../index.md) / [mozilla.components.concept.toolbar](../index.md) / [AutocompleteDelegate](index.md) / [applyAutocompleteResult](./apply-autocomplete-result.md)

# applyAutocompleteResult

`abstract fun applyAutocompleteResult(result: `[`AutocompleteResult`](../-autocomplete-result/index.md)`, onApplied: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/toolbar/src/main/java/mozilla/components/concept/toolbar/AutocompleteDelegate.kt#L18)

### Parameters

`result` - Apply result of autocompletion.

`onApplied` - a lambda/callback invoked if (and only if) the result has been
applied. A result may be discarded by implementations because it is stale or
the autocomplete request has been cancelled.