[android-components](../../index.md) / [mozilla.components.browser.domains.autocomplete](../index.md) / [DomainAutocompleteProvider](./index.md)

# DomainAutocompleteProvider

`interface DomainAutocompleteProvider` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/domains/src/main/java/mozilla/components/browser/domains/autocomplete/Providers.kt#L33)

### Functions

| Name | Summary |
|---|---|
| [getAutocompleteSuggestion](get-autocomplete-suggestion.md) | `abstract fun getAutocompleteSuggestion(query: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`DomainAutocompleteResult`](../-domain-autocomplete-result/index.md)`?` |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [BaseDomainAutocompleteProvider](../-base-domain-autocomplete-provider/index.md) | `open class BaseDomainAutocompleteProvider : `[`DomainAutocompleteProvider`](./index.md)`, CoroutineScope`<br>Provides common autocomplete functionality powered by domain lists. |
