[android-components](../../index.md) / [mozilla.components.feature.toolbar](../index.md) / [ToolbarAutocompleteFeature](./index.md)

# ToolbarAutocompleteFeature

`class ToolbarAutocompleteFeature` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/toolbar/src/main/java/mozilla/components/feature/toolbar/ToolbarAutocompleteFeature.kt#L22)

Feature implementation for connecting a toolbar with a list of autocomplete providers.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ToolbarAutocompleteFeature(toolbar: `[`Toolbar`](../../mozilla.components.concept.toolbar/-toolbar/index.md)`, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`? = null)`<br>Feature implementation for connecting a toolbar with a list of autocomplete providers. |

### Properties

| Name | Summary |
|---|---|
| [engine](engine.md) | `val engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`?`<br>(optional) instance of a browser [Engine](../../mozilla.components.concept.engine/-engine/index.md) to issue [Engine.speculativeConnect](../../mozilla.components.concept.engine/-engine/speculative-connect.md) calls on successful URL autocompletion. |
| [toolbar](toolbar.md) | `val toolbar: `[`Toolbar`](../../mozilla.components.concept.toolbar/-toolbar/index.md)<br>the [Toolbar](../../mozilla.components.concept.toolbar/-toolbar/index.md) to connect to autocomplete providers. |

### Functions

| Name | Summary |
|---|---|
| [addDomainProvider](add-domain-provider.md) | `fun addDomainProvider(provider: `[`DomainAutocompleteProvider`](../../mozilla.components.browser.domains.autocomplete/-domain-autocomplete-provider/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [addHistoryStorageProvider](add-history-storage-provider.md) | `fun addHistoryStorageProvider(provider: `[`HistoryStorage`](../../mozilla.components.concept.storage/-history-storage/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
