[android-components](../../index.md) / [mozilla.components.browser.domains](../index.md) / [DomainAutoCompleteProvider](./index.md)

# DomainAutoCompleteProvider

`class ~~DomainAutoCompleteProvider~~` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/domains/src/main/java/mozilla/components/browser/domains/DomainAutoCompleteProvider.kt#L24)
**Deprecated:** Use `ShippedDomainsProvider` or `CustomDomainsProvider`

Provides autocomplete functionality for domains, based on a provided list
of assets (see [Domains](../-domains/index.md)) and/or a custom domain list managed by
[CustomDomains](../-custom-domains/index.md).

### Types

| Name | Summary |
|---|---|
| [AutocompleteSource](-autocomplete-source/index.md) | `object AutocompleteSource` |
| [Result](-result/index.md) | `data class Result`<br>Represents a result of auto-completion. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DomainAutoCompleteProvider()`<br>Provides autocomplete functionality for domains, based on a provided list of assets (see [Domains](../-domains/index.md)) and/or a custom domain list managed by [CustomDomains](../-custom-domains/index.md). |

### Functions

| Name | Summary |
|---|---|
| [autocomplete](autocomplete.md) | `fun autocomplete(rawText: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Result`](-result/index.md)<br>Computes an autocomplete suggestion for the given text, and invokes the provided callback, passing the result. |
| [initialize](initialize.md) | `fun initialize(context: <ERROR CLASS>, useShippedDomains: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, useCustomDomains: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, loadDomainsFromDisk: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Initializes this provider instance by making sure the shipped and/or custom domains are loaded. |
