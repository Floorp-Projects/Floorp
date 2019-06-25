[android-components](../index.md) / [mozilla.components.browser.domains.autocomplete](./index.md)

## Package mozilla.components.browser.domains.autocomplete

### Types

| Name | Summary |
|---|---|
| [BaseDomainAutocompleteProvider](-base-domain-autocomplete-provider/index.md) | `open class BaseDomainAutocompleteProvider : `[`DomainAutocompleteProvider`](-domain-autocomplete-provider/index.md)`, CoroutineScope`<br>Provides common autocomplete functionality powered by domain lists. |
| [CustomDomainsProvider](-custom-domains-provider/index.md) | `class CustomDomainsProvider : `[`BaseDomainAutocompleteProvider`](-base-domain-autocomplete-provider/index.md)<br>Provides autocomplete functionality for domains based on a list managed by [CustomDomains](../mozilla.components.browser.domains/-custom-domains/index.md). |
| [DomainAutocompleteProvider](-domain-autocomplete-provider/index.md) | `interface DomainAutocompleteProvider` |
| [DomainAutocompleteResult](-domain-autocomplete-result/index.md) | `class DomainAutocompleteResult`<br>Describes an autocompletion result against a list of domains. |
| [DomainList](-domain-list/index.md) | `enum class DomainList` |
| [ShippedDomainsProvider](-shipped-domains-provider/index.md) | `class ShippedDomainsProvider : `[`BaseDomainAutocompleteProvider`](-base-domain-autocomplete-provider/index.md)<br>Provides autocomplete functionality for domains based on provided list of assets (see [Domains](../mozilla.components.browser.domains/-domains/index.md)). |

### Type Aliases

| Name | Summary |
|---|---|
| [DomainsLoader](-domains-loader.md) | `typealias DomainsLoader = (<ERROR CLASS>) -> `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Domain`](../mozilla.components.browser.domains/-domain/index.md)`>` |
