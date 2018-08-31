---
title: SessionUseCases.LoadUrlUseCase.invoke - 
---

[mozilla.components.feature.session](../../index.html) / [SessionUseCases](../index.html) / [LoadUrlUseCase](index.html) / [invoke](./invoke.html)

# invoke

`fun invoke(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, session: Session = sessionManager.selectedSessionOrThrow): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)

Loads the provided URL using the provided session (or the currently
selected session if none is provided).

### Parameters

`url` - url to load.

`session` - the session for which the URL should be loaded.