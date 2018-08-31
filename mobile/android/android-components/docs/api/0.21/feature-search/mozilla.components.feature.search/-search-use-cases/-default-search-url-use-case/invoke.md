---
title: SearchUseCases.DefaultSearchUrlUseCase.invoke - 
---

[mozilla.components.feature.search](../../index.html) / [SearchUseCases](../index.html) / [DefaultSearchUrlUseCase](index.html) / [invoke](./invoke.html)

# invoke

`fun invoke(searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, session: Session = sessionManager.selectedSessionOrThrow): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)

Triggers a search using the default search engine for the provided search terms.

### Parameters

`searchTerms` - the search terms.

`session` - the session to use, or the currently selected session if none
is provided.