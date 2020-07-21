[android-components](../index.md) / [mozilla.components.feature.search](./index.md)

## Package mozilla.components.feature.search

### Types

| Name | Summary |
|---|---|
| [BrowserStoreSearchAdapter](-browser-store-search-adapter/index.md) | `class BrowserStoreSearchAdapter : `[`SearchAdapter`](-search-adapter/index.md)<br>Adapter which wraps a [browserStore](#) in order to fulfill the [SearchAdapter](-search-adapter/index.md) interface. |
| [SearchAdapter](-search-adapter/index.md) | `interface SearchAdapter`<br>May be implemented by client code in order to allow a component to start searches. |
| [SearchFeature](-search-feature/index.md) | `class SearchFeature : `[`LifecycleAwareFeature`](../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)<br>Lifecycle aware feature that forwards [SearchRequest](../mozilla.components.concept.engine.search/-search-request/index.md)s from the [store](#) to [performSearch](#), and then consumes them. |
| [SearchUseCases](-search-use-cases/index.md) | `class SearchUseCases`<br>Contains use cases related to the search feature. |
