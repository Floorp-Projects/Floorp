[android-components](../../index.md) / [mozilla.components.browser.domains.autocomplete](../index.md) / [CustomDomainsProvider](./index.md)

# CustomDomainsProvider

`class CustomDomainsProvider : `[`BaseDomainAutocompleteProvider`](../-base-domain-autocomplete-provider/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/domains/src/main/java/mozilla/components/browser/domains/autocomplete/Providers.kt#L37)

Provides autocomplete functionality for domains based on a list managed by [CustomDomains](../../mozilla.components.browser.domains/-custom-domains/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `CustomDomainsProvider()`<br>Provides autocomplete functionality for domains based on a list managed by [CustomDomains](../../mozilla.components.browser.domains/-custom-domains/index.md). |

### Inherited Properties

| Name | Summary |
|---|---|
| [domains](../-base-domain-autocomplete-provider/domains.md) | `var domains: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Domain`](../../mozilla.components.browser.domains/-domain/index.md)`>` |

### Functions

| Name | Summary |
|---|---|
| [initialize](initialize.md) | `fun initialize(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inherited Functions

| Name | Summary |
|---|---|
| [getAutocompleteSuggestion](../-base-domain-autocomplete-provider/get-autocomplete-suggestion.md) | `open fun getAutocompleteSuggestion(query: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`DomainAutocompleteResult`](../-domain-autocomplete-result/index.md)`?`<br>Computes an autocomplete suggestion for the given text, and invokes the provided callback, passing the result. |
