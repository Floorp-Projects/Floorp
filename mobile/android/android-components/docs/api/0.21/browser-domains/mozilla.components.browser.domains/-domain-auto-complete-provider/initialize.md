---
title: DomainAutoCompleteProvider.initialize - 
---

[mozilla.components.browser.domains](../index.html) / [DomainAutoCompleteProvider](index.html) / [initialize](./initialize.html)

# initialize

`fun initialize(context: Context, useShippedDomains: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, useCustomDomains: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, loadDomainsFromDisk: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)

Initializes this provider instance by making sure the shipped and/or custom
domains are loaded.

### Parameters

`context` - the application context

`useShippedDomains` - true (default) if the domains provided by this
module should be used, otherwise false.

`useCustomDomains` - true if the custom domains provided by
[CustomDomains](../-custom-domains/index.html) should be used, otherwise false (default).

`loadDomainsFromDisk` - true (default) if domains should be loaded,
otherwise false. This parameter is for testing purposes only.