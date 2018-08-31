---
title: DomainAutoCompleteProvider - 
---

[mozilla.components.browser.domains](../index.html) / [DomainAutoCompleteProvider](./index.html)

# DomainAutoCompleteProvider

`class DomainAutoCompleteProvider`

Provides autocomplete functionality for domains, based on a provided list
of assets (see [Domains](../-domains/index.html)) and/or a custom domain list managed by
[CustomDomains](../-custom-domains/index.html).

### Types

| [AutocompleteSource](-autocomplete-source/index.html) | `object AutocompleteSource` |
| [Result](-result/index.html) | `data class Result`<br>Represents a result of auto-completion. |

### Constructors

| [&lt;init&gt;](-init-.html) | `DomainAutoCompleteProvider()`<br>Provides autocomplete functionality for domains, based on a provided list of assets (see [Domains](../-domains/index.html)) and/or a custom domain list managed by [CustomDomains](../-custom-domains/index.html). |

### Functions

| [autocomplete](autocomplete.html) | `fun autocomplete(rawText: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Result`](-result/index.html)<br>Computes an autocomplete suggestion for the given text, and invokes the provided callback, passing the result. |
| [initialize](initialize.html) | `fun initialize(context: Context, useShippedDomains: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, useCustomDomains: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, loadDomainsFromDisk: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Initializes this provider instance by making sure the shipped and/or custom domains are loaded. |

