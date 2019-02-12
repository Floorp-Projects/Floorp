[android-components](../../../index.md) / [mozilla.components.feature.search](../../index.md) / [SearchUseCases](../index.md) / [DefaultSearchUseCase](index.md) / [invoke](./invoke.md)

# invoke

`fun invoke(searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/search/src/main/java/mozilla/components/feature/search/SearchUseCases.kt#L40)

Overrides [SearchUseCase.invoke](../-search-use-case/invoke.md)

Triggers a search in the currently selected session.

`fun invoke(searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, session: `[`Session`](../../../mozilla.components.browser.session/-session/index.md)`? = sessionManager.selectedSession): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/search/src/main/java/mozilla/components/feature/search/SearchUseCases.kt#L51)

Triggers a search using the default search engine for the provided search terms.

### Parameters

`searchTerms` - the search terms.

`session` - the session to use, or the currently selected session if none
is provided.