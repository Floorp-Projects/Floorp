[android-components](../../../index.md) / [mozilla.components.feature.search](../../index.md) / [SearchUseCases](../index.md) / [SearchUseCase](./index.md)

# SearchUseCase

`interface SearchUseCase` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/search/src/main/java/mozilla/components/feature/search/SearchUseCases.kt#L28)

### Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `abstract fun invoke(searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, searchEngine: `[`SearchEngine`](../../../mozilla.components.browser.search/-search-engine/index.md)`? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [DefaultSearchUseCase](../-default-search-use-case/index.md) | `class DefaultSearchUseCase : `[`SearchUseCase`](./index.md) |
| [NewTabSearchUseCase](../-new-tab-search-use-case/index.md) | `class NewTabSearchUseCase : `[`SearchUseCase`](./index.md) |
