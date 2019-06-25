[android-components](../../index.md) / [mozilla.components.browser.domains](../index.md) / [DomainAutoCompleteProvider](index.md) / [initialize](./initialize.md)

# initialize

`fun initialize(context: <ERROR CLASS>, useShippedDomains: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, useCustomDomains: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, loadDomainsFromDisk: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/domains/src/main/java/mozilla/components/browser/domains/DomainAutoCompleteProvider.kt#L91)

Initializes this provider instance by making sure the shipped and/or custom
domains are loaded.

### Parameters

`context` - the application context

`useShippedDomains` - true (default) if the domains provided by this
module should be used, otherwise false.

`useCustomDomains` - true if the custom domains provided by
[CustomDomains](../-custom-domains/index.md) should be used, otherwise false (default).

`loadDomainsFromDisk` - true (default) if domains should be loaded,
otherwise false. This parameter is for testing purposes only.