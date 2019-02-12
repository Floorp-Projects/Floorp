[android-components](../../index.md) / [mozilla.components.browser.domains.autocomplete](../index.md) / [BaseDomainAutocompleteProvider](./index.md)

# BaseDomainAutocompleteProvider

`abstract class BaseDomainAutocompleteProvider : `[`DomainAutocompleteProvider`](../-domain-autocomplete-provider/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/domains/src/main/java/mozilla/components/browser/domains/autocomplete/Providers.kt#L52)

Provides common autocomplete functionality powered by domain lists.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BaseDomainAutocompleteProvider(list: `[`DomainList`](../-domain-list/index.md)`)`<br>Provides common autocomplete functionality powered by domain lists. |

### Properties

| Name | Summary |
|---|---|
| [domains](domains.md) | `var domains: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Domain`](../../mozilla.components.browser.domains/-domain/index.md)`>` |

### Functions

| Name | Summary |
|---|---|
| [getAutocompleteSuggestion](get-autocomplete-suggestion.md) | `open fun getAutocompleteSuggestion(query: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`DomainAutocompleteResult`](../-domain-autocomplete-result/index.md)`?`<br>Computes an autocomplete suggestion for the given text, and invokes the provided callback, passing the result. |
| [initialize](initialize.md) | `abstract fun initialize(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [CustomDomainsProvider](../-custom-domains-provider/index.md) | `class CustomDomainsProvider : `[`BaseDomainAutocompleteProvider`](./index.md)<br>Provides autocomplete functionality for domains based on a list managed by [CustomDomains](../../mozilla.components.browser.domains/-custom-domains/index.md). |
| [ShippedDomainsProvider](../-shipped-domains-provider/index.md) | `class ShippedDomainsProvider : `[`BaseDomainAutocompleteProvider`](./index.md)<br>Provides autocomplete functionality for domains based on provided list of assets (see [Domains](../../mozilla.components.browser.domains/-domains/index.md)). |
