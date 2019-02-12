[android-components](../../index.md) / [mozilla.components.feature.toolbar](../index.md) / [ToolbarAutocompleteFeature](./index.md)

# ToolbarAutocompleteFeature

`class ToolbarAutocompleteFeature` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/toolbar/src/main/java/mozilla/components/feature/toolbar/ToolbarAutocompleteFeature.kt#L17)

Feature implementation for connecting a toolbar with a list of autocomplete providers.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ToolbarAutocompleteFeature(toolbar: `[`Toolbar`](../../mozilla.components.concept.toolbar/-toolbar/index.md)`)`<br>Feature implementation for connecting a toolbar with a list of autocomplete providers. |

### Properties

| Name | Summary |
|---|---|
| [toolbar](toolbar.md) | `val toolbar: `[`Toolbar`](../../mozilla.components.concept.toolbar/-toolbar/index.md) |

### Functions

| Name | Summary |
|---|---|
| [addDomainProvider](add-domain-provider.md) | `fun addDomainProvider(provider: `[`DomainAutocompleteProvider`](../../mozilla.components.browser.domains.autocomplete/-domain-autocomplete-provider/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [addHistoryStorageProvider](add-history-storage-provider.md) | `fun addHistoryStorageProvider(provider: `[`HistoryStorage`](../../mozilla.components.concept.storage/-history-storage/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
