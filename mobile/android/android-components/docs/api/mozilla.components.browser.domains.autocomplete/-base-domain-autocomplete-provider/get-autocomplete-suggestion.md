[android-components](../../index.md) / [mozilla.components.browser.domains.autocomplete](../index.md) / [BaseDomainAutocompleteProvider](index.md) / [getAutocompleteSuggestion](./get-autocomplete-suggestion.md)

# getAutocompleteSuggestion

`open fun getAutocompleteSuggestion(query: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`DomainAutocompleteResult`](../-domain-autocomplete-result/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/domains/src/main/java/mozilla/components/browser/domains/autocomplete/Providers.kt#L70)

Overrides [DomainAutocompleteProvider.getAutocompleteSuggestion](../-domain-autocomplete-provider/get-autocomplete-suggestion.md)

Computes an autocomplete suggestion for the given text, and invokes the
provided callback, passing the result.

### Parameters

`query` - text to be auto completed

**Return**
the result of auto-completion, or null if no match is found.

