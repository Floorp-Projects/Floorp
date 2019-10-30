[android-components](../../index.md) / [mozilla.components.browser.domains.autocomplete](../index.md) / [ShippedDomainsProvider](./index.md)

# ShippedDomainsProvider

`class ShippedDomainsProvider : `[`BaseDomainAutocompleteProvider`](../-base-domain-autocomplete-provider/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/domains/src/main/java/mozilla/components/browser/domains/autocomplete/Providers.kt#L26)

Provides autocomplete functionality for domains based on provided list of assets (see [Domains](../../mozilla.components.browser.domains/-domains/index.md)).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ShippedDomainsProvider()`<br>Provides autocomplete functionality for domains based on provided list of assets (see [Domains](../../mozilla.components.browser.domains/-domains/index.md)). |

### Inherited Properties

| Name | Summary |
|---|---|
| [domains](../-base-domain-autocomplete-provider/domains.md) | `var domains: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Domain`](../../mozilla.components.browser.domains/-domain/index.md)`>` |

### Inherited Functions

| Name | Summary |
|---|---|
| [getAutocompleteSuggestion](../-base-domain-autocomplete-provider/get-autocomplete-suggestion.md) | `open fun getAutocompleteSuggestion(query: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`DomainAutocompleteResult`](../-domain-autocomplete-result/index.md)`?`<br>Computes an autocomplete suggestion for the given text, and invokes the provided callback, passing the result. |
| [initialize](../-base-domain-autocomplete-provider/initialize.md) | `fun initialize(context: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [launchGeckoResult](../../mozilla.components.browser.engine.gecko/kotlinx.coroutines.-coroutine-scope/launch-gecko-result.md) | `fun <T> CoroutineScope.launchGeckoResult(context: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = EmptyCoroutineContext, start: CoroutineStart = CoroutineStart.DEFAULT, block: suspend CoroutineScope.() -> `[`T`](../../mozilla.components.browser.engine.gecko/kotlinx.coroutines.-coroutine-scope/launch-gecko-result.md#T)`): `[`GeckoResult`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoResult.html)`<`[`T`](../../mozilla.components.browser.engine.gecko/kotlinx.coroutines.-coroutine-scope/launch-gecko-result.md#T)`>`<br>Create a GeckoResult from a co-routine. |
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
